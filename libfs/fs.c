#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

#define FAT_EOC 65535

//Superblock Struct
struct __attribute__((__packed__)) superblock{
	char		signature[8];
	int16_t		totalBlocks;
	int16_t		rootDirectoryIndex;
	int16_t		dataBlockStartIndex;
	int16_t		dataBlockTotal;
	int8_t		numFATBlocks;
	uint8_t		padding[BLOCK_SIZE - 17];
};

//Root Struct
struct __attribute__((__packed__)) root{
	char		fileName[FS_FILENAME_LEN];
	uint32_t	fileSize;
	uint16_t	dataBlockStartIndex;
	uint8_t		padding[10];
};

//File Table
struct __attribute__((__packed__)) fileTable{
	char 		entry[FS_OPEN_MAX_COUNT][FS_FILENAME_LEN];
	size_t 		offset[FS_OPEN_MAX_COUNT];
	int size;
};

//Initialize Data Structs
struct superblock 	superblock;
uint16_t		*FAT;
struct root		root[FS_FILE_MAX_COUNT];
struct fileTable	fileTable;

//Checks for available space in root
int checkRootSize(){
	int size = 0;
	int i;

	//Loops through the entire root directory
	for(i = 0; i < FS_FILE_MAX_COUNT; i++){
		//Checks for the NULL character in ASCII
		if(root[i].fileName[0] == 0)
			size++;
	}
	return size;
}

//Checks for available space in FAT
int checkFATSize(){
	int size = 0;
	int i;

	//Loops through the entire FAT
	for(i = 0; i < superblock.dataBlockTotal; i++){
		//Checks for FAT_EOC
		if(FAT[i] == 0)
			size++;
	}
	return size;
}

//Checks that file exists in root 
int checkRootFile(const char* fileName){
	int i;
	//Loops through the entire root directory
	for(i = 0; i < FS_FILE_MAX_COUNT; i++){
		//Checks for the NULL character in ASCII
		if(strcmp(fileName, root[i].fileName) == 0)
			return 0;
	}

	return -1;
}

//Gets Data Block index for a specific file
int getRootIndex(const char* fileName){
	int i;
	//Loops through the entire root directory
	for(i = 0; i < FS_FILE_MAX_COUNT; i++){
		//Checks for the NULL character in ASCII
		if(strcmp(fileName, root[i].fileName) == 0)
			return i;
	}

	return -1;
}

//return next available index in FAT
//otherwise return -1
int getFATIndex(){
	int index = -1;

	//Loops through the entire FAT
	for(int i = 1; i < superblock.dataBlockTotal + 1; i++){
		//Checks for FAT_EOC
		if(FAT[i] == 0){
			index = i;
			break;
		}
	}

	//cannot be more than total number of data blocks
	if(index == superblock.dataBlockTotal)
		index = -1;

	return index;
}


int fs_mount(const char *diskname)
{
	int testBlockOpen = block_disk_open(diskname);
	int i;

	//if open fails, return -1
	if(testBlockOpen == -1)
		return -1;

	//initialize filetable size
	fileTable.size = 0;
	//Read all data for superblock
	block_read(0, &superblock);
	//Error Checking
	if(strncmp(superblock.signature, "ECS150FS", 8) != 0 || 
		block_disk_count() != superblock.totalBlocks)
		return -1;

	//Reads FAT and root Directory from disk
	FAT = (uint16_t *) malloc(superblock.numFATBlocks * BLOCK_SIZE);
	for(i = 1; i < superblock.numFATBlocks + 1; i++)
		block_read(i, &FAT[i - 1]);

	block_read(superblock.rootDirectoryIndex, &root);
	return 0;
}

int fs_umount(void)
{
	int i;
	if(strncmp(superblock.signature, "ECS150FS", 8) != 0 || 
		block_disk_count() != superblock.totalBlocks)
		return -1;

	block_write(0, &superblock);
	//Reads FAT and root Directory from disk
	for(i = 1; i < superblock.numFATBlocks + 1; i++)
		block_write(i, &FAT[i - 1]);

	free(FAT);
	block_write(superblock.rootDirectoryIndex, &root);

	if(block_disk_close() == -1)
		return -1;

	return 0;
}

int fs_info(void)
{
	if(strncmp(superblock.signature, "ECS150FS", 8) != 0 || 
		block_disk_count() != superblock.totalBlocks)
		return -1;

	printf("FS Info:\n");
	printf("total_blk_count=%d\n", superblock.totalBlocks);
	printf("fat_blk_count=%d\n", superblock.numFATBlocks);
	printf("rdir_blk=%d\n", superblock.rootDirectoryIndex);
	printf("data_blk=%d\n", superblock.dataBlockStartIndex);
	printf("data_blk_count=%d\n", superblock.dataBlockTotal);
	printf("fat_free_ratio=%d/%d\n", checkFATSize(),
					superblock.dataBlockTotal);
	printf("rdir_free_ratio=%d/%d\n", checkRootSize(), FS_FILE_MAX_COUNT);

	return 0;
}

int fs_create(const char *filename)
{
	int i;
	if(!filename || strlen(filename) > FS_FILENAME_LEN ||
		checkRootSize() == 0 || filename[strlen(filename)] != '\0'
		|| strncmp(superblock.signature, "ECS150FS", 8) != 0)
		return -1;

	for(i = 0; i < FS_FILE_MAX_COUNT; i++){
		//if file already exists, fail
		if(strcmp(root[i].fileName, filename) == 0)
			return -1;

		//if entry is empty then copy file name
		if(root[i].fileName[0] == 0){
			strcpy(root[i].fileName, filename);
			root[i].fileSize = 0;
			root[i].dataBlockStartIndex = FAT_EOC;
			break;
		}
	}

	return 0;
}

int fs_delete(const char *filename)
{
	int index, nextIndex, i;
	int startIndex = superblock.dataBlockStartIndex;
	uint8_t clearBuffer[BLOCK_SIZE];

	memset(clearBuffer, 0, BLOCK_SIZE); //create empty array

	if(!filename || strlen(filename) > FS_FILENAME_LEN
		|| strncmp(superblock.signature, "ECS150FS", 8) != 0)
		return -1;

	for(i = 0; i < FS_FILE_MAX_COUNT; i++){
		//if file already exists, reset it
		if(strcmp(root[i].fileName, filename) == 0){
			strcpy(root[i].fileName, "\0");
			index = root[i].dataBlockStartIndex;

			//goes through each FAT entry until end
			while(FAT[index] != FAT_EOC){
				nextIndex = FAT[index]; //get the next data block index
				FAT[index] = 0;
				block_write(startIndex+index, clearBuffer); //clear data block
				index = nextIndex;
			}

			FAT[index] = 0;
			block_write(startIndex+index, clearBuffer); //clear data block
			return 0;
		}
	}

	return -1;
}

int fs_ls(void)
{
	int i = 0;
	
	if(strncmp(superblock.signature, "ECS150FS", 8) != 0)
		return -1;

	printf("FS Ls:\n");

	for(i = 0; i < FS_FILE_MAX_COUNT; i++){
		if(root[i].fileName[0] != 0)
			printf("file: %s, size: %d, data_blk: %d\n", 
						root[i].fileName,
						root[i].fileSize,
						root[i].dataBlockStartIndex);
	}

	return 0;
}

int fs_open(const char *filename)
{
	int fd = 0;

	if(fileTable.size == FS_OPEN_MAX_COUNT || !filename || 
		strlen(filename) > FS_FILENAME_LEN || checkRootFile(filename)
		|| strncmp(superblock.signature, "ECS150FS", 8) != 0)
		return -1;

	//goes through each entry in filetable to find an empty file name
	while(fileTable.entry[fd][0] != 0)
		fd++;

	//add to file table
	strcpy(fileTable.entry[fd], filename);	
	fileTable.offset[fd] = 0;	
	fileTable.size++;
	return fd;
}

int fs_close(int fd)
{
	if(fileTable.entry[fd][0] == '\0' || fd > FS_OPEN_MAX_COUNT ||
		fd < 0 || strncmp(superblock.signature, "ECS150FS", 8) != 0)
		return -1;

	strcpy(fileTable.entry[fd], "\0");
	fileTable.size--;
	return 0;
}

int fs_stat(int fd)
{
	int i;
	const char* fileName = fileTable.entry[fd];	

	//error management
	if(fileTable.entry[fd][0] == '\0' || fd > FS_OPEN_MAX_COUNT ||
		fd < 0 || strncmp(superblock.signature, "ECS150FS", 8) != 0)
		return -1;

	//search through root directory for file 
	for(i = 0; i < FS_FILE_MAX_COUNT; i++){
		//Checks for the NULL character in ASCII
		if(strcmp(fileName, root[i].fileName) == 0)
			return root[i].fileSize;
	}

	return -1;
}

int fs_lseek(int fd, size_t offset)
{
	//check that fd is valid
	if(fileTable.entry[fd][0] == '\0' || fd > FS_OPEN_MAX_COUNT ||
		fd < 0 || strncmp(superblock.signature, "ECS150FS", 8) != 0)
		return -1;
	
	fileTable.offset[fd] = offset;	

	return 0;
}

int fs_write(int fd, void *buf, size_t count)
{
	size_t offset, blockBytesLeft;
	uint8_t backBuffer[count], bounceBuffer[BLOCK_SIZE];
	int dataIndex, tempIndex,startIndex = superblock.dataBlockStartIndex;
	int bytesWritten = 0;
	int currentRootIndex;

	//check that fd is valid
	if(fileTable.entry[fd][0] == '\0' || fd > FS_OPEN_MAX_COUNT ||
		fd < 0 || strncmp(superblock.signature, "ECS150FS", 8) != 0)
		return -1;

	memcpy(backBuffer, buf, count);

	//Finds File in Root and returns Root
	//Then finds its offset
	currentRootIndex = getRootIndex(fileTable.entry[fd]);
	dataIndex = root[currentRootIndex].dataBlockStartIndex;

	if(dataIndex == FAT_EOC){
		//set root's starting index to available index
		root[currentRootIndex].dataBlockStartIndex = dataIndex = getFATIndex();
		
		if(dataIndex == -1)
			return 0;

		//set entry in FAT to FAT_EOC;
		FAT[dataIndex]=FAT_EOC;

	}

	offset = fileTable.offset[fd];

	//Calculate number of bytes left in the block
	blockBytesLeft = BLOCK_SIZE - (offset % BLOCK_SIZE); 

	//Calculates the current index with offset
	//Then Reads into Bounce buffer
	dataIndex = dataIndex + (offset / BLOCK_SIZE);
	block_read(startIndex + dataIndex, &bounceBuffer);
	while(count !=  0){
		//Or resets current bytes left and goes to next block
		if(FAT[dataIndex] != FAT_EOC && blockBytesLeft == 0){
			block_write(startIndex + dataIndex, &bounceBuffer);
			blockBytesLeft = BLOCK_SIZE;
			memset(bounceBuffer, 0, BLOCK_SIZE); //reset bounceBuffer 
			dataIndex = FAT[dataIndex];
			block_read(startIndex + dataIndex, &bounceBuffer);
		}
		//Extending file, adding more data blocks
		else if(FAT[dataIndex] == FAT_EOC && blockBytesLeft == 0) {
			block_write(startIndex + dataIndex, &bounceBuffer);
			blockBytesLeft = BLOCK_SIZE;
			memset(bounceBuffer, 0, BLOCK_SIZE); //reset bounceBuffer 
			//find available data block
			tempIndex = getFATIndex();
			//no data blocks availabe. FAT full
			if(tempIndex == -1){
				FAT[dataIndex] = FAT_EOC; //current data block will be the last
				dataIndex = -1;
				break;
			}
			FAT[dataIndex] = tempIndex;
			dataIndex = tempIndex;
			FAT[dataIndex] = FAT_EOC;
			block_read(startIndex + dataIndex, &bounceBuffer);
		}
		//copies into backBuffer first
		bounceBuffer[BLOCK_SIZE - blockBytesLeft] = backBuffer[bytesWritten];
		bytesWritten++;
		fileTable.offset[fd]++;
		blockBytesLeft--;
		count--;
	}

	if(dataIndex != -1){
		block_write(startIndex + dataIndex, &bounceBuffer);
		FAT[dataIndex] = FAT_EOC;
	}

	if(root[currentRootIndex].fileSize == 0)
		root[currentRootIndex].fileSize = bytesWritten;
	else if(offset + bytesWritten > root[currentRootIndex].fileSize)
		root[currentRootIndex].fileSize = offset + bytesWritten;

	return bytesWritten;
}

int fs_read(int fd, void *buf, size_t count)
{
	size_t offset, currentBytesLeft, blockBytesLeft;
	uint8_t backBuffer[count], bounceBuffer[BLOCK_SIZE];
	int dataIndex, startIndex = superblock.dataBlockStartIndex;
	int bytesRead = 0;
	int rootIndex;
	uint32_t fileSize;

	//check that fd is valid
	if(fileTable.entry[fd][0] == '\0' || fd > FS_OPEN_MAX_COUNT ||
		fd < 0 || strncmp(superblock.signature, "ECS150FS", 8) != 0)
		return -1;

	//Finds File in Root and returns Root
	//Then finds its offset
	rootIndex = getRootIndex(fileTable.entry[fd]);
	dataIndex = root[rootIndex].dataBlockStartIndex;
	fileSize = root[rootIndex].fileSize;

	offset = fileTable.offset[fd];

	//Calculates numBytes left in the file
	currentBytesLeft = fileSize - offset;

	//Calculate number of bytes left in the block
	blockBytesLeft = BLOCK_SIZE - (offset % BLOCK_SIZE); 

	//Calculates the current index with offset
	//Then Reads into Bounce buffer
	dataIndex = dataIndex + (offset / BLOCK_SIZE);
	block_read(startIndex + dataIndex, &bounceBuffer);

	while((currentBytesLeft != 0) && (count !=  0)){
		//Or resets current bytes left and goes to next block
		if(FAT[dataIndex] != FAT_EOC && blockBytesLeft == 0){
			blockBytesLeft = BLOCK_SIZE;
			dataIndex = FAT[dataIndex];
			block_read(startIndex+ dataIndex, &bounceBuffer);
		}
		//copies into backBuffer first
		backBuffer[bytesRead] = bounceBuffer[BLOCK_SIZE - blockBytesLeft];
		bytesRead++;
		fileTable.offset[fd]++;
		blockBytesLeft--;
		count--;
		currentBytesLeft--;
	}

	//read it all back into the buf
	memcpy(buf, backBuffer, bytesRead);
	return bytesRead;
}

