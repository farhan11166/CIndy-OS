[BITS 32]

; Write 'X' with bright red on white (color 0x4F) to bottom-right corner
; Row 24, Col 79 => offset = (24 * 80 + 79) * 2 = 3998 => 0xB8000 + 3998 = 0xB8F9E
mov word [0xB8F9E], 0x4F58

ret
