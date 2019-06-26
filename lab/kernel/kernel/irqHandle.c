#include "x86.h"
#include "device.h"
#include "fs.h"

#define SYS_OPEN 0
#define SYS_WRITE 1
#define SYS_READ 2
#define SYS_LSEEK 3
#define SYS_CLOSE 4
#define SYS_REMOVE 5
#define SYS_FORK 6
#define SYS_EXEC 7
#define SYS_SLEEP 8
#define SYS_EXIT 9
#define SYS_SEM 10

#define STD_OUT 0
#define STD_IN 1

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define SEM_INIT 0
#define SEM_WAIT 1
#define SEM_POST 2
#define SEM_DESTROY 3

#define O_WRITE 0x01
#define O_READ 0x02
#define O_CREATE 0x04
#define O_DIRECTORY 0x08

extern TSS tss;

extern ProcessTable pcb[MAX_PCB_NUM];
extern int current;

extern Semaphore sem[MAX_SEM_NUM];
extern Device dev[MAX_DEV_NUM];
extern File file[MAX_FILE_NUM];

extern SuperBlock sBlock;
extern GroupDesc gDesc[MAX_GROUP_NUM];

extern int displayRow;
extern int displayCol;

extern uint32_t keyBuffer[MAX_KEYBUFFER_SIZE];
extern int bufferHead;
extern int bufferTail;

void GProtectFaultHandle(struct StackFrame *sf);
void timerHandle(struct StackFrame *sf);
void keyboardHandle(struct StackFrame *sf);
void syscallHandle(struct StackFrame *sf);

void syscallOpen(struct StackFrame *sf);
void syscallWrite(struct StackFrame *sf);
void syscallRead(struct StackFrame *sf);
void syscallLseek(struct StackFrame *sf);
void syscallClose(struct StackFrame *sf);
void syscallRemove(struct StackFrame *sf);
void syscallFork(struct StackFrame *sf);
void syscallExec(struct StackFrame *sf);
void syscallSleep(struct StackFrame *sf);
void syscallExit(struct StackFrame *sf);
void syscallSem(struct StackFrame *sf);

int recRemove(Inode* destInode,Inode* fatherInode,int* destInodeOffset,int* fatherInodeOffset,char* fileName);

void syscallWriteStdOut(struct StackFrame *sf);
void syscallWriteFile(struct StackFrame *sf);

void syscallReadStdIn(struct StackFrame *sf);
void syscallReadFile(struct StackFrame *sf);

void syscallSemInit(struct StackFrame *sf);
void syscallSemWait(struct StackFrame *sf);
void syscallSemPost(struct StackFrame *sf);
void syscallSemDestroy(struct StackFrame *sf);

void irqHandle(struct StackFrame *sf) { // pointer sf = esp
	/* Reassign segment register */
	asm volatile("movw %%ax, %%ds"::"a"(KSEL(SEG_KDATA)));
	/*XXX Save esp to stackTop */
	uint32_t tmpStackTop = pcb[current].stackTop;
	pcb[current].prevStackTop = pcb[current].stackTop;
	pcb[current].stackTop = (uint32_t)sf;

	switch(sf->irq) {
		case -1:
			break;
		case 0xd:
			GProtectFaultHandle(sf);
			break;
		case 0x20:
			timerHandle(sf);
			break;
		case 0x21:
			keyboardHandle(sf);
			break;
		case 0x80:
			syscallHandle(sf);
			break;
		default:assert(0);
	}
	/*XXX Recover stackTop */
	pcb[current].stackTop = tmpStackTop;
}

void GProtectFaultHandle(struct StackFrame *sf) {
	assert(0);
	return;
}

void timerHandle(struct StackFrame *sf) {
	int i;
	uint32_t tmpStackTop;
	i = (current+1) % MAX_PCB_NUM;
	while (i != current) {
		if (pcb[i].state == STATE_BLOCKED && pcb[i].sleepTime != -1) {
			pcb[i].sleepTime --;
			if (pcb[i].sleepTime == 0)
				pcb[i].state = STATE_RUNNABLE;
		}
		i = (i+1) % MAX_PCB_NUM;
	}

	if (pcb[current].state == STATE_RUNNING &&
		pcb[current].timeCount != MAX_TIME_COUNT) {
		pcb[current].timeCount++;
		return;
	}
	else {
		if (pcb[current].state == STATE_RUNNING) {
			pcb[current].state = STATE_RUNNABLE;
			pcb[current].timeCount = 0;
		}
		
		i = (current+1) % MAX_PCB_NUM;
		while (i != current) {
			if (i !=0 && pcb[i].state == STATE_RUNNABLE)
				break;
			i = (i+1) % MAX_PCB_NUM;
		}
		if (pcb[i].state != STATE_RUNNABLE)
			i = 0;
		current = i;
		/*XXX echo pid of selected process */
		//putChar('0'+current);
		pcb[current].state = STATE_RUNNING;
		pcb[current].timeCount = 1;
		/*XXX recover stackTop of selected process */
		tmpStackTop = pcb[current].stackTop;
		pcb[current].stackTop = pcb[current].prevStackTop;
		tss.esp0 = (uint32_t)&(pcb[current].stackTop); // setting tss for user process
		asm volatile("movl %0, %%esp"::"m"(tmpStackTop)); // switch kernel stack
		asm volatile("popl %gs");
		asm volatile("popl %fs");
		asm volatile("popl %es");
		asm volatile("popl %ds");
		asm volatile("popal");
		asm volatile("addl $8, %esp");
		asm volatile("iret");
	}
}

void keyboardHandle(struct StackFrame *sf) {
	ProcessTable *pt = NULL;
	uint32_t keyCode = getKeyCode();
	if (keyCode == 0) // illegal keyCode
		return;
	//putChar(getChar(keyCode));
	keyBuffer[bufferTail] = keyCode;
	bufferTail=(bufferTail+1)%MAX_KEYBUFFER_SIZE;
	if (dev[STD_IN].value < 0) { // with process blocked
		dev[STD_IN].value ++;

		pt = (ProcessTable*)((uint32_t)(dev[STD_IN].pcb.prev) -
					(uint32_t)&(((ProcessTable*)0)->blocked));
		pt->state = STATE_RUNNABLE;
		pt->sleepTime = 0;

		dev[STD_IN].pcb.prev = (dev[STD_IN].pcb.prev)->prev;
		(dev[STD_IN].pcb.prev)->next = &(dev[STD_IN].pcb);
	}
	return;
}

void syscallHandle(struct StackFrame *sf) {
	switch(sf->eax) { // syscall number
		case SYS_OPEN:
			syscallOpen(sf);
			break; // for SYS_OPEN
		case SYS_WRITE:
			syscallWrite(sf);
			break; // for SYS_WRITE
		case SYS_READ:
			syscallRead(sf);
			break; // for SYS_READ
		case SYS_LSEEK:
			syscallLseek(sf);
			break; // for SYS_SEEK
		case SYS_CLOSE:
			syscallClose(sf);
			break; // for SYS_CLOSE
		case SYS_REMOVE:
			syscallRemove(sf);
			break; // for SYS_REMOVE
		case SYS_FORK:
			syscallFork(sf);
			break; // for SYS_FORK
		case SYS_EXEC:
			syscallExec(sf);
			break; // for SYS_EXEC
		case SYS_SLEEP:
			syscallSleep(sf);
			break; // for SYS_SLEEP
		case SYS_EXIT:
			syscallExit(sf);
			break; // for SYS_EXIT
		case SYS_SEM:
			syscallSem(sf);
			break; // for SYS_SEM
		default:break;
	}
}

void syscallOpen(struct StackFrame *sf) {
	int i;
	//char tmp = 0;
	int length = 0;
	//int cond = 0;
	int ret = 0;
	int size = 0;
	int baseAddr = (current + 1) * 0x100000; // base address of user process
	char *str = (char*)sf->ecx + baseAddr; // file path
	char fatherPath[NAME_LENGTH*10];
	char fileName[NAME_LENGTH];
	//int count=0;
	//DirEntry *dirEntry = NULL;
	Inode fatherInode;
	Inode destInode;
	int fatherInodeOffset = 0;
	int destInodeOffset = 0;

	//XXX
	readGroupHeader(&sBlock, gDesc);

	ret = readInode(&sBlock, gDesc, &destInode, &destInodeOffset, str);
    
    // STEP 2
    // TODO: try to complete file open
    /** consider following situations
    if file exit (ret == 0) {
        if INODE type != file open mode
            do someting
        if the file refer to a device in use
            do something
        if the file refer to a file in use
            do something
        if the file refer to a file not in use
            do something
        if no available file
            do something
    }
    else {
        if O_CREATE not set
            do something
        if O_DIRECTORY not set
            do something
        else
            do something
        if ret == -1
            do something
        create file success or fail
    */
   int isCreate=sf->edx&O_CREATE;
   int isDir=sf->edx&O_DIRECTORY;
   int isWrite=sf->edx&O_WRITE;
   if(isDir&&isWrite){
	   pcb[current].regs.eax=-1;
	   return;
   }
   if(ret==0){
	   if((destInode.type==DIRECTORY_TYPE&&!isDir)||(destInode.type!=DIRECTORY_TYPE&&isDir)){
		   pcb[current].regs.eax=-1;
		   return;
	   }
	   for(i=0;i<MAX_DEV_NUM;i++){
		   if(dev[i].state&&dev[i].inodeOffset==destInodeOffset){
			   	dev[i].value--;
			    if(dev[i].value<0){
					pcb[current].blocked.next = sem[i].pcb.next;
					pcb[current].blocked.prev = &(sem[i].pcb);
					dev[i].pcb.next = &(pcb[current].blocked);
					(pcb[current].blocked.next)->prev = &(pcb[current].blocked);
					
					pcb[current].state = STATE_BLOCKED;
					pcb[current].sleepTime = -1;
					asm volatile("int $0x20");
				}
				pcb[current].regs.eax = i;
				return;
		   }
	   }

		/*It is not supposed to prevent opening a file in use*/
	   /*
	   for(i=0;i<MAX_FILE_NUM;i++){
		   if(file[i].state&&file[i].inodeOffset==destInodeOffset){
				pcb[current].regs.eax=-1;
		   	    return;
		   }
	   }
	   */

	   for(i=0;i<MAX_FILE_NUM;i++){
		   if(!file[i].state){
			   file[i].state=1;
			   file[i].inodeOffset=destInodeOffset;
			   file[i].flags=sf->edx;
			   file[i].offset=0;
			   pcb[current].regs.eax=i+MAX_DEV_NUM;
			   return;
		   }
	   }
	   if(i==MAX_FILE_NUM){
		   pcb[current].regs.eax=-1;
		   return;
	   }
   }
   else{
	   /****************************fix bug: problems when create directory*****************************/
		if(!isCreate){
			pcb[current].regs.eax=-1;
			return;
		}
		length=stringLen(str);
		if(str[length-1]=='/'){
			stringCpy(str,fatherPath,length-1);
			length--;
			if(!isDir){
				pcb[current].regs.eax=-1;
				return;
			}
		}
		else{
			stringCpy(str,fatherPath,length);
		}
		stringChrR(fatherPath,'/',&size);
		stringCpy(str,fatherPath,size+1);
		stringCpy(str+size+1,fileName,length-size-1);

		//XXX
		readGroupHeader(&sBlock, gDesc);

		ret = readInode(&sBlock, gDesc, &fatherInode, &fatherInodeOffset, fatherPath);
		if(ret==0){
			for(i=0;i<MAX_FILE_NUM;i++){
				if(!file[i].state){
					break;
				}
			}
			if(i==MAX_FILE_NUM){
				pcb[current].regs.eax=-1;
				return;
			}

			//XXX
			readGroupHeader(&sBlock, gDesc);

			ret=allocInode(&sBlock,gDesc,&fatherInode,fatherInodeOffset,&destInode,&destInodeOffset,fileName,isDir?DIRECTORY_TYPE:REGULAR_TYPE);
			if(ret==0){
				file[i].state=1;
				file[i].inodeOffset=destInodeOffset;
				file[i].flags=sf->edx;
				file[i].offset=0;
				pcb[current].regs.eax=i+MAX_DEV_NUM;
				return;
			}
			else{
				pcb[current].regs.eax=-1;
				return;
			}
		}
		else{
			pcb[current].regs.eax=-1;
			return;
		}
   }
	return;
}

void syscallWrite(struct StackFrame *sf) {
	switch(sf->ecx) { // file descriptor
		case STD_OUT:
			if (dev[STD_OUT].state == 1)
				syscallWriteStdOut(sf);
			break; // for STD_OUT
		default:break;
	}
	if (sf->ecx >= MAX_DEV_NUM && sf->ecx < MAX_DEV_NUM + MAX_FILE_NUM) {
		if (file[sf->ecx - MAX_DEV_NUM].state == 1)
			syscallWriteFile(sf);
	}
	return;
}

void syscallWriteStdOut(struct StackFrame *sf) {
	int sel = sf->ds; //TODO segment selector for user data, need further modification
	char *str = (char*)sf->edx;
	int size = sf->ebx;
	int i = 0;
	int pos = 0;
	char character = 0;
	uint16_t data = 0;
	asm volatile("movw %0, %%es"::"m"(sel));
	for (i = 0; i < size; i++) {
		asm volatile("movb %%es:(%1), %0":"=r"(character):"r"(str+i));
		if(character == '\n') {
			displayRow++;
			displayCol=0;
			if(displayRow==MAX_ROW){
				displayRow=MAX_ROW-1;
				displayCol=0;
				scrollScreen();
			}
		}
		else {
			data = character | (0x0c << 8);
			pos = (MAX_COL*displayRow+displayCol)*2;
			asm volatile("movw %0, (%1)"::"r"(data),"r"(pos+0xb8000));
			displayCol++;
			if(displayCol==MAX_COL){
				displayRow++;
				displayCol=0;
				if(displayRow==MAX_ROW){
					displayRow=MAX_ROW-1;
					displayCol=0;
					scrollScreen();
				}
			}
		}
		//asm volatile("int $0x20"); //XXX Testing irqTimer during syscall, consistent problem between register and memory can occur
		//asm volatile("int $0x20":::"memory"); //XXX memory option tell the compiler not to sort the instructions (i.e., previous instructions are all finished) and not to cache memory in register
	}
	
	updateCursor(displayRow, displayCol);
	//TODO take care of return value
	pcb[current].regs.eax = size;
	return;
}

void syscallWriteFile(struct StackFrame *sf) {
	if (file[sf->ecx - MAX_DEV_NUM].flags % 2 == 0) { //XXX if O_WRITE is not set
		pcb[current].regs.eax = -1;
		return;
	}

	int i = 0;
	int j = 0;
	int ret = 0;
	int already=0;
	int baseAddr = (current + 1) * 0x100000; // base address of user process
	uint8_t *str = (uint8_t*)sf->edx + baseAddr; // buffer of user process
	int size = sf->ebx;
	uint8_t buffer[SECTOR_SIZE * SECTORS_PER_BLOCK];
	int quotient = file[sf->ecx - MAX_DEV_NUM].offset / sBlock.blockSize;
	int remainder = file[sf->ecx - MAX_DEV_NUM].offset % sBlock.blockSize;

	Inode inode;
	diskRead(&inode, sizeof(Inode), 1, file[sf->ecx - MAX_DEV_NUM].inodeOffset);
	
	if (size <= 0) {
		pcb[current].regs.eax = 0;
		return;
	}
	
    // STEP 3
    // TODO: try to complete file write
    /** condider as many situations as you can
    */
   	if(inode.type==DIRECTORY_TYPE||file[sf->ecx-MAX_DEV_NUM].offset>inode.size){
		pcb[current].regs.eax=-1;
		return;
	}
	i=remainder;
	
	//XXX
	readGroupHeader(&sBlock, gDesc);

	while(j<size){
		already=j;
		if(quotient>=inode.blockCount){
			ret=allocBlock(&sBlock,gDesc,&inode,file[sf->ecx - MAX_DEV_NUM].inodeOffset);
			if(ret==-1){
				break;
			}
		}
		ret=readBlock(&sBlock,&inode,quotient,buffer);
		if(ret==-1){
			break;
		}
		for(;i<SECTOR_SIZE * SECTORS_PER_BLOCK;i++){
			buffer[i]=str[j];
			j++;
			file[sf->ecx - MAX_DEV_NUM].offset++;
			if(j==size){
				break;
			}
		}
		i=0;
		ret=writeBlock(&sBlock,&inode,quotient,buffer);
		if(ret==-1){
			break;
		}
		already=j;
		quotient++;
	}

	/************************fix the bug: I should update inode's size***************************/
	if(file[sf->ecx-MAX_DEV_NUM].offset>inode.size){
		inode.size=file[sf->ecx-MAX_DEV_NUM].offset;
		diskWrite(&inode, sizeof(Inode), 1, file[sf->ecx - MAX_DEV_NUM].inodeOffset);
	}

	pcb[current].regs.eax=already;
	return;
}

void syscallRead(struct StackFrame *sf) {
	switch(sf->ecx) { // file descriptor
		case STD_IN:
			if (dev[STD_IN].state == 1)
				syscallReadStdIn(sf);
			break; // for STD_IN
		default:break;
	}
	if (sf->ecx >= MAX_DEV_NUM && sf->ecx < MAX_DEV_NUM + MAX_FILE_NUM) { // for file
		if (file[sf->ecx - MAX_DEV_NUM].state == 1)
			syscallReadFile(sf);
	}
}

void syscallReadStdIn(struct StackFrame *sf) {
	//TODO more than one process request for STD_IN
	if (dev[STD_IN].value == 0) { // no process blocked
		/*XXX Blocked for I/O */
		dev[STD_IN].value --;

		pcb[current].blocked.next = dev[STD_IN].pcb.next;
		pcb[current].blocked.prev = &(dev[STD_IN].pcb);
		dev[STD_IN].pcb.next = &(pcb[current].blocked);
		(pcb[current].blocked.next)->prev = &(pcb[current].blocked);

		pcb[current].state = STATE_BLOCKED;
		pcb[current].sleepTime = -1; // blocked on STD_IN

		bufferHead = bufferTail;
		asm volatile("int $0x20");
		/*XXX Resumed from Blocked */
		int sel = sf->ds;
		char *str = (char*)sf->edx;
		int size = sf->ebx; // MAX_BUFFER_SIZE, reverse last byte
		int i = 0;
		char character = 0;
		asm volatile("movw %0, %%es"::"m"(sel));
		while(i < size-1) {
			if(bufferHead != bufferTail){ //XXX what if keyBuffer is overflow
				character = getChar(keyBuffer[bufferHead]);
				bufferHead = (bufferHead+1)%MAX_KEYBUFFER_SIZE;
				putChar(character);
				if(character != 0) {
					asm volatile("movb %0, %%es:(%1)"::"r"(character),"r"(str+i));
					i++;
				}
			}
			else
				break;
		}
		asm volatile("movb $0x00, %%es:(%0)"::"r"(str+i));
		pcb[current].regs.eax = i;
		return;
	}
	else if (dev[STD_IN].value < 0) { // with process blocked
		pcb[current].regs.eax = -1;
		return;
	}
}

void syscallReadFile(struct StackFrame *sf) {
	if ((file[sf->ecx - MAX_DEV_NUM].flags >> 1) % 2 == 0) { //XXX if O_READ is not set
		pcb[current].regs.eax = -1;
		return;
	}
	int ret;
	int i = 0;
	int j = 0;
	int baseAddr = (current + 1) * 0x100000; // base address of user process
	uint8_t *str = (uint8_t*)sf->edx + baseAddr; // buffer of user process
	int size = sf->ebx; // MAX_BUFFER_SIZE, don't need to reserve last byte
	uint8_t buffer[SECTOR_SIZE * SECTORS_PER_BLOCK];
	int quotient = file[sf->ecx - MAX_DEV_NUM].offset / sBlock.blockSize;
	int remainder = file[sf->ecx - MAX_DEV_NUM].offset % sBlock.blockSize;
	
	Inode inode;
	diskRead(&inode, sizeof(Inode), 1, file[sf->ecx - MAX_DEV_NUM].inodeOffset);
	
    // STEP 4
    // TODO: try to complete file read
    /** consider different size
    */
	if(file[sf->ecx-MAX_DEV_NUM].offset>inode.size){
		pcb[current].regs.eax=-1;
		return;
	}
	if(size>inode.size-file[sf->ecx-MAX_DEV_NUM].offset){
		size=inode.size-file[sf->ecx-MAX_DEV_NUM].offset;
	}
	if (size <= 0) {
		pcb[current].regs.eax = 0;
		return;
	}

	//XXX
	readGroupHeader(&sBlock, gDesc);

	i=remainder;
	while(j<size){
		if(quotient>=inode.blockCount){
			pcb[current].regs.eax=j;
			return;
		}
		ret=readBlock(&sBlock,&inode,quotient,buffer);
		if(ret==-1){
			pcb[current].regs.eax=j;
			return;
		}
		for(;i<SECTOR_SIZE * SECTORS_PER_BLOCK;i++){
			str[j]=buffer[i];
			j++;
			file[sf->ecx - MAX_DEV_NUM].offset++;
			if(j==size){
				break;
			}
			if(file[sf->ecx-MAX_DEV_NUM].offset==inode.size){
				pcb[current].regs.eax=j;   
				return;
			}
		}
		i=0;
		quotient++;
	}
	pcb[current].regs.eax=size;   
	return;
}

void syscallLseek(struct StackFrame *sf) {
    // STEP 5
    // TODO: try to complete seek
    /**
    consider different file type and different whence
    */
    if (sf->ecx >= MAX_DEV_NUM && sf->ecx < MAX_DEV_NUM + MAX_FILE_NUM) {
		if (file[sf->ecx - MAX_DEV_NUM].state == 1){
			int target=-1;
			Inode inode;
			diskRead(&inode, sizeof(Inode), 1, file[sf->ecx - MAX_DEV_NUM].inodeOffset);
			switch(sf->ebx){
				case SEEK_SET:
					target=sf->edx;
					break;
				case SEEK_CUR:
					target=sf->edx+file[sf->ecx - MAX_DEV_NUM].offset;
					break;
				case SEEK_END:
					target=sf->edx+inode.size;
					break;
				default:
					break;
			}
			if(target>=0&&target<=inode.size){
				file[sf->ecx - MAX_DEV_NUM].offset=target;
				pcb[current].regs.eax=target;
				return;
			}
			else{
				pcb[current].regs.eax=-1;
				return;
			}
		}
	}
	pcb[current].regs.eax=-1;
	return;
}

void syscallClose(struct StackFrame *sf) {
    // STEP 6
    // TODO: try to complete file close
    /**
    if file is dev or out of range
        do something
    if file not in use
        do something
    close file
    */
	if(sf->ecx<0||sf->ecx>=MAX_DEV_NUM+MAX_FILE_NUM){
		pcb[current].regs.eax=-1;
		return;
	}
	if(sf->ecx<MAX_DEV_NUM){
		if(dev[sf->ecx].state){
			dev[sf->ecx].value++;
			if(dev[sf->ecx].value<=0){
				ProcessTable *pt = NULL;
				pt = (ProcessTable*)((uint32_t)(dev[sf->ecx].pcb.prev) -
				(uint32_t)&(((ProcessTable*)0)->blocked));
				pt->state = STATE_RUNNABLE;
				pt->sleepTime = 0;
				dev[sf->ecx].pcb.prev = (dev[sf->ecx].pcb.prev)->prev;
				(dev[sf->ecx].pcb.prev)->next = &(dev[sf->ecx].pcb);
			}
			pcb[current].regs.eax=0;
			return;
		}
		pcb[current].regs.eax=-1;
		return;	
	}
	else{
		if(file[sf->ecx-MAX_DEV_NUM].state==0){
			pcb[current].regs.eax=-1;
			return;
		}
		else{
			file[sf->ecx-MAX_DEV_NUM].state=0;
			pcb[current].regs.eax=0;
			return;
		}
	}
}

void syscallRemove(struct StackFrame *sf) {
	int i;
	//char tmp = 0;
	int length = 0;
	//int cond = 0;
	int ret = 0;
	int size = 0;
	int baseAddr = (current + 1) * 0x100000; // base address of user process
	char *str = (char*)sf->ecx + baseAddr; // file path
	Inode fatherInode;
	Inode destInode;
	int fatherInodeOffset = 0;
	int destInodeOffset = 0;
	char fatherPath[NAME_LENGTH*10];
	char fileName[NAME_LENGTH];

	//XXX
	readGroupHeader(&sBlock, gDesc);

	ret = readInode(&sBlock, gDesc, &destInode, &destInodeOffset, str);
    
    // STEP 7
    // TODO: try to complete file remove
    /**
    if file exist {
        if file refer to a device
            do
        if file refer to a file in use
            do
        remove file (different file type, success or not)
    else
        do
    }
    */
   	if(ret==0){
		for(i=0;i<MAX_DEV_NUM;i++){
		   	if(dev[i].state&&dev[i].inodeOffset==destInodeOffset){
			   	pcb[current].regs.eax=-1;
			   	return;
		   	}
		}
		for(i=0;i<MAX_FILE_NUM;i++){
			if(file[i].state==1&&file[i].inodeOffset==destInodeOffset){
				pcb[current].regs.eax=-1;
			   	return;
			}
		}
		length=stringLen(str);
		if(str[length-1]=='/'){
			stringCpy(str,fatherPath,length-1);
			length--;
		}
		else{
			stringCpy(str,fatherPath,length);
		}
		stringChrR(fatherPath,'/',&size);
		stringCpy(str,fatherPath,size+1);
		stringCpy(str+size+1,fileName,length-size-1);
		readInode(&sBlock, gDesc, &fatherInode, &fatherInodeOffset, fatherPath);
		ret=recRemove(&destInode,&fatherInode,&destInodeOffset,&fatherInodeOffset,fileName);
		pcb[current].regs.eax=ret;
		return;
	}
	else{
		pcb[current].regs.eax=-1;
		return;
	}
}

int recRemove(Inode* destInode,Inode* fatherInode,int* destInodeOffset,int* fatherInodeOffset,char* fileName){
	int ret;
	int i;
	int j;
	uint8_t buffer[BLOCK_SIZE];
	int nextInodeOffset;
	Inode nextInode;
	DirEntry *dirEntry = NULL;

	//XXX
	readGroupHeader(&sBlock, gDesc);

	if(destInode->type!=DIRECTORY_TYPE){
		return freeInode(&sBlock,gDesc,fatherInode,*fatherInodeOffset,destInode,destInodeOffset,fileName,destInode->type);
	}
	int blockCount = destInode->blockCount;
	for (i = 0; i < blockCount; i ++) {
		ret = readBlock(&sBlock, destInode, i, buffer);
		if (ret == -1)
			return -1;
		dirEntry = (DirEntry*)buffer;
		for (j = 0; j < sBlock.blockSize / sizeof(DirEntry); j ++) {
			if (dirEntry[j].inode == 0){ 
				continue;
			}
			else{
				nextInodeOffset = dirEntry[j].inode;
				diskRead(&nextInode, sizeof(Inode), 1, nextInodeOffset);
				ret=recRemove(&nextInode,destInode,&nextInodeOffset,destInodeOffset,dirEntry[j].name);
				if(ret==-1){
					return -1;
				}
			}
		}
	}

	//XXX
	readGroupHeader(&sBlock, gDesc);
	
	return freeInode(&sBlock,gDesc,fatherInode,*fatherInodeOffset,destInode,destInodeOffset,fileName,destInode->type);
}

void syscallFork(struct StackFrame *sf) {
	int i, j;
	for (i = 0; i < MAX_PCB_NUM; i++) {
		if (pcb[i].state == STATE_DEAD)
			break;
	}
	if (i != MAX_PCB_NUM) {
		/*XXX copy userspace
		  XXX enable interrupt
		 */
		enableInterrupt();
		for (j = 0; j < 0x100000; j++) {
			*(uint8_t *)(j + (i+1)*0x100000) = *(uint8_t *)(j + (current+1)*0x100000);
			//asm volatile("int $0x20"); //XXX Testing irqTimer during syscall
		}
		/*XXX disable interrupt
		 */
		disableInterrupt();
		/*XXX set pcb
		  XXX pcb[i]=pcb[current] doesn't work
		*/
		pcb[i].stackTop = (uint32_t)&(pcb[i].stackTop) -
			((uint32_t)&(pcb[current].stackTop) - pcb[current].stackTop);
		pcb[i].prevStackTop = (uint32_t)&(pcb[i].stackTop) -
			((uint32_t)&(pcb[current].stackTop) - pcb[current].prevStackTop);
		pcb[i].state = STATE_RUNNABLE;
		pcb[i].timeCount = pcb[current].timeCount;
		pcb[i].sleepTime = pcb[current].sleepTime;
		pcb[i].pid = i;
		/*XXX set regs */
		pcb[i].regs.ss = USEL(2+i*2);
		pcb[i].regs.esp = pcb[current].regs.esp;
		pcb[i].regs.eflags = pcb[current].regs.eflags;
		pcb[i].regs.cs = USEL(1+i*2);
		pcb[i].regs.eip = pcb[current].regs.eip;
		pcb[i].regs.eax = pcb[current].regs.eax;
		pcb[i].regs.ecx = pcb[current].regs.ecx;
		pcb[i].regs.edx = pcb[current].regs.edx;
		pcb[i].regs.ebx = pcb[current].regs.ebx;
		pcb[i].regs.xxx = pcb[current].regs.xxx;
		pcb[i].regs.ebp = pcb[current].regs.ebp;
		pcb[i].regs.esi = pcb[current].regs.esi;
		pcb[i].regs.edi = pcb[current].regs.edi;
		pcb[i].regs.ds = USEL(2+i*2);
		pcb[i].regs.es = pcb[current].regs.es;
		pcb[i].regs.fs = pcb[current].regs.fs;
		pcb[i].regs.gs = pcb[current].regs.gs;
		/*XXX set return value */
		pcb[i].regs.eax = 0;
		pcb[current].regs.eax = i;
	}
	else {
		pcb[current].regs.eax = -1;
	}
	return;
}

void syscallExec(struct StackFrame *sf) {
	return;
}

void syscallSleep(struct StackFrame *sf) {
	if (sf->ecx == 0)
		return;
	else {
		pcb[current].state = STATE_BLOCKED;
		pcb[current].sleepTime = sf->ecx;
		asm volatile("int $0x20");
		return;
	}
}

void syscallExit(struct StackFrame *sf) {
	pcb[current].state = STATE_DEAD;
	asm volatile("int $0x20");
	return;
}

void syscallSem(struct StackFrame *sf) {
	switch(sf->ecx) {
		case SEM_INIT:
			syscallSemInit(sf);
			break;
		case SEM_WAIT:
			syscallSemWait(sf);
			break;
		case SEM_POST:
			syscallSemPost(sf);
			break;
		case SEM_DESTROY:
			syscallSemDestroy(sf);
			break;
		default:break;
	}
}

void syscallSemInit(struct StackFrame *sf) {
	int i;
	for (i = 0; i < MAX_SEM_NUM ; i++) {
		if (sem[i].state == 0) // not in use
			break;
	}
	if (i != MAX_SEM_NUM) {
		sem[i].state = 1;
		sem[i].value = (int32_t)sf->edx;
		sem[i].pcb.next = &(sem[i].pcb);
		sem[i].pcb.prev = &(sem[i].pcb);
		pcb[current].regs.eax = i;
	}
	else
		pcb[current].regs.eax = -1;
	return;
}

void syscallSemWait(struct StackFrame *sf) {
	int i = (int)sf->edx;
	if (i < 0 || i >= MAX_SEM_NUM) {
		pcb[current].regs.eax = -1;
		return;
	}
	if (sem[i].state == 0) { // not in use
		pcb[current].regs.eax = -1;
		return;
	}
	if (sem[i].value >= 1) { // not to block itself
		sem[i].value --;
		pcb[current].regs.eax = 0;
		return;
	}
	if (sem[i].value < 1) { // block itself on this sem
		sem[i].value --;
		pcb[current].blocked.next = sem[i].pcb.next;
		pcb[current].blocked.prev = &(sem[i].pcb);
		sem[i].pcb.next = &(pcb[current].blocked);
		(pcb[current].blocked.next)->prev = &(pcb[current].blocked);
		
		pcb[current].state = STATE_BLOCKED;
		pcb[current].sleepTime = -1;
		asm volatile("int $0x20");
		pcb[current].regs.eax = 0;
		return;
	}
}

void syscallSemPost(struct StackFrame *sf) {
	int i = (int)sf->edx;
	ProcessTable *pt = NULL;
	if (i < 0 || i >= MAX_SEM_NUM) {
		pcb[current].regs.eax = -1;
		return;
	}
	if (sem[i].state == 0) { // not in use
		pcb[current].regs.eax = -1;
		return;
	}
	if (sem[i].value >= 0) { // no process blocked
		sem[i].value ++;
		pcb[current].regs.eax = 0;
		return;
	}
	if (sem[i].value < 0) { // release process blocked on this sem 
		sem[i].value ++;

		pt = (ProcessTable*)((uint32_t)(sem[i].pcb.prev) -
					(uint32_t)&(((ProcessTable*)0)->blocked));
		pt->state = STATE_RUNNABLE;
		pt->sleepTime = 0;

		sem[i].pcb.prev = (sem[i].pcb.prev)->prev;
		(sem[i].pcb.prev)->next = &(sem[i].pcb);
		
		pcb[current].regs.eax = 0;
		return;
	}
}

void syscallSemDestroy(struct StackFrame *sf) {
	int i = (int)sf->edx;
	if (i < 0 || i >= MAX_SEM_NUM) {
		pcb[current].regs.eax = -1;
		return;
	}
	if (sem[i].state == 0) { // not in use
		pcb[current].regs.eax = -1;
		return;
	}
	sem[i].state = 0;
	sem[i].value = 0;
	sem[i].pcb.next = &(sem[i].pcb);
	sem[i].pcb.prev = &(sem[i].pcb);
	pcb[current].regs.eax = 0;
	return;
}
