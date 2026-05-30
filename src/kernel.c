void print(const char* msg,int row){
    char *video_memory = (char*)0xb8000;
    int offset=row*80*2;
    for(int i=0;msg[i]!='\0';i++){

        video_memory[offset+ (i*2)] = msg[i];
        video_memory[offset+ (i*2)+1] = 0x6F;
    

    }
}
void clear_screen() {
    char *video_memory = (char*) 0xb8000;

    for (int i = 0; i < 80 * 25; i++) {
        video_memory[i * 2] = ' ';
        video_memory[i * 2 + 1] = 0x00;
    }
}
void kernel_main(){
    clear_screen();
    print("Welcome to CIndy-os",1);
    print("Boot Succesful",0);

}