# Bug Report — PMM / kmalloc_a Frame Aliasing Causing Page Fault in `run` Command

**Date:** 2026-08-31  
**Severity:** Critical — user-mode execution always crashes  
**Status:** Fixed  
**Files Changed:** `src/memory.c`, `include/memory.h`, `src/keyboard.c`

---

## Symptom

Running `run YOOO.BIN` from the kernel shell printed the expected load messages and
jumped to ring 3, but immediately caused a page fault:

```
run: flat binary loaded at 0x40000000
Loaded 53 bytes, jumping to entry 0x0X40000000...
jumping to ring 3 at 0x0X40000000...
EXCEPTION: Page Fault
  EIP: 0X40001000   CS: 0X0000001B
  EBP: 0X00000004
  CR2 (fault addr): 0X40001000
  Cause: not present, read, user mode
System Halted!
```

The fault always landed at `0x40001000` — the second page of the user binary's virtual
region — despite YOOO.BIN being only 53 bytes long and needing only a single 4 KB page.

---

## Investigation

### Step 1 — Read the map_page debug output

The `map_page` debug prints showed:

```
map_page: created page table for PD index 0X00000100,
          mapped vaddr 0x0X40000000
       -> phys 0x0X00222000
          PDE=0x0X00222007  PTE=0x0X00222007
```

**PDE and PTE are identical: `0x00222007`.**  
This means the page table itself and the user data page both occupy physical frame
`0x222000`. The same physical frame was handed out twice.

### Step 2 — Trace the allocation sequence

The `run` command (flat binary path in `keyboard.c`) does:

```c
uint8_t *tmp_buf = kmalloc_a(4096);          // (1)
fat16_read_file(..., tmp_buf, 4096);          // (2) YOOO.bin read into tmp_buf
...
uint32_t phys = pmm_alloc_block();            // (3)
map_page(phys, 0x40000000u, 7u);             // (4) — creates page table internally
memset((void *)0x40000000u, 0, 4096u);       // (5)
memcpy((void *)0x40000000u, img, result);    // (6)
```

Walk through with actual addresses:

| Step | Action | Frame affected |
|------|--------|----------------|
| (1) | `kmalloc_a(4096)` returns `0x222000`, advances `free_mem_addr` to `0x223000` | Frame `0x222` owned by bump allocator — **PMM bitmap NOT updated** |
| (2) | YOOO.bin read into `tmp_buf` @ `0x222000` | Frame `0x222` holds the binary |
| (3) | `pmm_alloc_block()` scans bitmap, sees frame `0x222` marked free → returns `0x222000` | Frame `0x222` aliased — same memory as `tmp_buf`! |
| (4) | `map_page(0x222000, 0x40000000, 7)` — PD index `0x100` has no table yet, calls `pmm_alloc_block()` internally for the page table → gets frame `0x223` | Page table @ `0x223000`, PTE[0] = `0x222007` |
| (5) | `memset((void*)0x40000000, 0, 4096)` — writes through vaddr `0x40000000` to phys `0x222000` — **zeroes `tmp_buf`** | YOOO.bin destroyed |
| (6) | `memcpy((void*)0x40000000, img, 53)` — `img = tmp_buf = 0x222000` which is now all zeros — **copies 53 zero bytes** | User page contains no code |

The user page ends up filled with `0x00` bytes. `0x00 0x00` decodes as `add byte [eax], al`.
With undefined register values from ring-3 entry, the CPU immediately traps somewhere
— in this run, it coincidentally faulted at the unmapped second page `0x40001000`.

> **Note on the screenshot:** the `map_page` debug output in the screenshot showed
> `PDE == PTE == 0x222007`.  This indicates the *page table* frame was also
> `0x222000` — meaning the internal `pmm_alloc_block()` inside `map_page` also
> returned the same frame.  This happens when `free_mem_addr` after boot lands exactly
> on a frame boundary and both consecutive `pmm_alloc_block()` calls (for user page and
> for page table) draw from the same untracked window.  The exact frame numbers vary
> per run, but the aliasing class of bug is the same regardless of which scenario
> applies.

### Step 3 — Confirm the root cause: PMM is never told about kmalloc_a allocations

In `memory.c`, `init_pmm` marks every frame from `0` to `ceil(free_mem_addr / 4096)` as
used at boot time.  But **any `kmalloc_a` call that happens after `init_pmm` has run
silently advances `free_mem_addr` without touching the bitmap**.

```c
// init_pmm marks up to here:
uint32_t kernel_frames = (free_mem_addr + 4095) / 4096;
for (uint32_t i = 0; i < kernel_frames; i++) {
    pmm_set_bit(i);        // only knows about allocations up to this moment
}

// Later, kmalloc_a does this:
void* ptr = (void*)free_mem_addr;
free_mem_addr += size;
return ptr;                // PMM bitmap never touched — frame is "free" in bitmap
```

Every `kmalloc_a` call after boot (e.g. the `tmp_buf` allocation in `run`) produces a
frame that `pmm_alloc_block()` will happily hand out again to the next caller.

---

## Root Cause Summary

> `kmalloc_a` uses a bump allocator but does not register its allocations in the PMM
> bitmap.  `init_pmm` only marks frames up to the `free_mem_addr` value at the moment
> it runs.  Any `kmalloc_a` call after `init_pmm` creates an invisible allocation that
> `pmm_alloc_block()` treats as free physical memory.  In the `run` command, this causes
> the file-read buffer (`tmp_buf`) and the user page frame to be the same physical
> memory.  The subsequent `memset` of the mapped user page destroys the source binary
> before `memcpy` can read it.

---

## Fix

### Fix 1 — `pmm_mark_used()` hooked into `kmalloc_a` (`src/memory.c`, `include/memory.h`)

Added a function that marks any physical range as used in the PMM bitmap:

```c
void pmm_mark_used(uint32_t phys_addr, uint32_t size) {
    if (size == 0) return;
    uint32_t first_frame = phys_addr / 4096;
    uint32_t last_frame  = (phys_addr + size - 1) / 4096;
    for (uint32_t i = first_frame; i <= last_frame; i++) {
        pmm_set_bit(i);
    }
}
```

`kmalloc_a` now calls it immediately after advancing `free_mem_addr`:

```c
void* kmalloc_a(unsigned int size) {
    if (free_mem_addr & 0xFFF) {
        free_mem_addr &= 0xFFFFF000;
        free_mem_addr += 0x1000;
    }
    void* ptr = (void*)free_mem_addr;
    free_mem_addr += size;
    allocated_memory += size;
    pmm_mark_used((uint32_t)ptr, size);   // ← NEW: keep bitmap in sync
    return ptr;
}
```

This is safe to call even before `init_pmm` runs — the `init_pmm` init pass will
re-mark everything anyway via its `kernel_frames` loop.  After `init_pmm`, all future
`kmalloc_a` allocations are immediately visible to the PMM.

### Fix 2 — `memcpy` before `memset` in the flat binary path (`src/keyboard.c`)

The old order was: map → zero → copy.  The new order is: map → copy → zero tail.

```c
// OLD (broken):
map_page(phys, 0x40000000u, 7u);
memset((void *)0x40000000u, 0, 4096u);        // zeroes tmp_buf if frames alias
memcpy((void *)0x40000000u, img, result);     // copies from zeroed buffer

// NEW (correct):
map_page(phys, 0x40000000u, 7u);
memcpy((void *)0x40000000u, img, (uint32_t)result);   // copy binary first
if ((uint32_t)result < 4096u) {
    memset((void *)(0x40000000u + result), 0,          // zero only the tail
           4096u - (uint32_t)result);
}
```

Even if frame aliasing were to reoccur for any reason, the binary is already safely
written to the destination before anything touches the source.

---

## Why the Fault Was at `0x40001000` Specifically

The fault address `0x40001000` — exactly the second page — is explained by the TLB:

- `map_page` installed a valid PTE for `0x40000000` and called `invlpg`, so the TLB
  cached the mapping for page 0.
- The CPU executed instructions through the zeroed page 0 (TLB hit, no page-table walk
  needed).
- Eventually execution jumped or fell into page 1 (`0x40001000`), causing a TLB miss.
  The CPU walked the page table and found the PTE for index 1 to be `0x00000000`
  (present bit = 0) — because the page table only ever mapped index 0.
- Page fault, "not present, user mode."

---

## Lessons

1. **Bump allocators and bitmap allocators must be kept in sync.**  If both exist in the
   same kernel, every bump allocation must immediately mark its frames in the bitmap.
   Letting `init_pmm` do a one-time snapshot at boot is not enough once `kmalloc_a` is
   called at runtime.

2. **Never `memset` a destination before reading the source when they can alias.**  If
   the source and destination share physical memory, zeroing the destination destroys the
   source.  Always copy first, zero the leftover tail after.

3. **The TLB can mask page-table corruption.**  A corrupted page table does not always
   crash immediately on the first access.  If a prior `invlpg` installed a TLB entry,
   accesses to that cached page succeed silently until the TLB entry is evicted or a
   different page causes a miss.  This is why the crash appeared at `0x40001000` (second
   page, TLB miss) rather than `0x40000000` (first page, TLB hit from `invlpg`).

---

## Related Entries

- **Error Log §3** — `run` command triple fault (PMM `max_frames = 0`)
- **Error Log §5** — `map_page` page fault after kernel grows past 4 MB (identity map
  too small)
- **Error Log §6** — Ring-3 crash from invalid TSS descriptor and stale exit stack
