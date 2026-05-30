void kernel_main(){
    char *video_memory = (char*)0xb8000;
    const char *msg="Welcome to Cindy-OS";
    for(int i=0;msg[i]!='\0';i++){

        video_memory[i*2] = msg[i];
        video_memory[i*2+1] = 0x6F;
    

    }



}