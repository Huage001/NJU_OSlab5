#include "types.h"
#include "utils.h"
#include "lib.h"


union DirEntry {
	uint8_t byte[DIRENTRY_SIZE];
	struct {
		int32_t inode;
		char name[64];
	};
};

typedef union DirEntry DirEntry;

int ls(char *destFilePath) {
	// STEP 8
    // TODO: ls
    /** output format
    ls /
    boot dev usr
    */
   	int fd;
	int ret;
	int size;
	uint8_t buffer[MAX_BUFFER_SIZE];
	int i;
	DirEntry* dirEntry;
    printf("ls %s\n",destFilePath);
	fd=open(destFilePath,O_READ|O_DIRECTORY);
	if(fd==-1){
		printf("ls failed!\n");
		return -1;
	}
	size=lseek(fd,0,SEEK_END);
	lseek(fd,0,SEEK_SET);
	for(i=0;i<size/DIRENTRY_SIZE;i++){
		ret=read(fd,buffer,DIRENTRY_SIZE);
		if(ret==-1){
			printf("\nls failed!\n");
			return -1;
		}
		dirEntry = (DirEntry*)buffer;
		if(dirEntry->inode!=0){
			printf("%s ",dirEntry->name);
		}
	}
	ret=close(fd);
	printf("\nls success!\n");
	return 0;
}

int cat(char *destFilePath) {
	// STEP 9
    // TODO: ls
    /** output format
    cat /usr/test
    ABCDEFGHIJKLMNOPQRSTUVWXYZ
    */
    int fd;
	int ret;
	uint8_t buffer[MAX_BUFFER_SIZE];
    printf("cat %s\n",destFilePath);
	fd=open(destFilePath,O_READ);
	if(fd==-1){
		printf("cat failed!\n");
		return -1;
	}
	while(1){
		ret=read(fd,buffer,MAX_BUFFER_SIZE/2);
		if(ret==-1){
			printf("\ncat failed!\n");
			return -1;
		}
		if(ret==0){
			break;
		}
		buffer[ret]=0;
		printf("%s",buffer);
	}
	close(fd);
	printf("\ncat success!\n");
	return 0;
}

int uEntry(void) {
    // STEP 2-9
    // TODO: try different test case when you finish a function
    
    /* filesystem test */
	int fd = 0;
	int i = 0;
	char tmp = 0;
	
	ls("/");
	ls("/boot/");
	ls("/dev/");
	ls("/usr/");

	printf("create /usr/test and write alphabets to it\n");
	fd = open("/usr/test", O_WRITE | O_READ | O_CREATE);
	//write(fd, (uint8_t*)"Hello, World!\n", 14);
	//write(fd, (uint8_t*)"This is a demo!\n", 16);
	//for (i = 0; i < 2049; i ++) {
	//for (i = 0; i < 2048; i ++) {
	//for (i = 0; i < 1025; i ++) {
	//for (i = 0; i < 512; i ++) {
	for (i = 0; i < 26; i ++) {
		tmp = (char)(i % 26 + 'A');
		write(fd, (uint8_t*)&tmp, 1);
	}
	close(fd);
	cat("/usr/test");
	printf("\n");
	printf("rm /usr/test\n");
	remove("/usr/test");
	ls("/usr/");
	printf("rmdir /usr/\n");
	remove("/usr/");
	//remove("/dev");
	ls("/");
	//ls("/dev");
	printf("create /usr/\n");
	fd = open("/usr/", O_CREATE | O_DIRECTORY);
	close(fd);
	ls("/");
	//fd = open("/usr/test", O_WRITE | O_READ);
	//close(fd);
	//ls("/usr");
	//fd = open("/usr/test/", O_CREATE);
	//close(fd);
	//ls("/usr/");
	//fd = open("/usr/test", O_CREATE);
	//close(fd);
	//ls("/usr/");
	
	exit();
    /**
    Output format can be found in 
    http://114.212.80.195:8170/2019oslab/lab5/
    */
	return 0;
}
