#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <strings.h>

#define SYSTEM_SIZE (16 * 1024 * 1024)
#define INODE_SIZE 160
#define INODE_COUNT 512
#define BLOCK_COUNT (SYSTEM_SIZE/512)
#define INODE_START 8
#define DATA_START 179


typedef struct inode
{
	struct stat info; //see stat struct man page
	unsigned char direct[12];
	unsigned char indirect[4];
}inode;

typedef struct super
{
	unsigned char inode_bitmap[64];
	unsigned char data_bitmap[4074]; //TODO: might lower this at some point (by 1)
}super;

super superBlock;

void setMetadata(); //initialize metadata for first use of filesystem
inode get_inode(char*, int); //given a file path and starting inode (directory), traverse directories to find inode
