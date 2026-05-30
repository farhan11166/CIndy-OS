void print(const char* msg,int row){
    char *video_memory = (char*)0xb8000;
    int offset=row*80*2;
    for(int i=0;msg[i]!='\0';i++){

        video_memory[offset+ (i*2)] = msg[i];
        video_memory[offset+ (i*2)+1] = 0x6F;
    

    }
}
void kernel_main(){
    print("Welcome to CIndy-os",1);
    print("Boot Succesful",0);

}