#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <strings.h>
#include <math.h>

#define SYSTEM_SIZE (16 * 1024 * 1024)
#define INODE_COUNT 512
#define BLOCK_COUNT (SYSTEM_SIZE/512)
#define INODE_START 8
#define DATA_START 520
#define ROOT_PATH "/tmp/laf224/mountdir"


typedef struct inode
{
	struct stat info; //see stat struct man page
	unsigned short direct[32];
	unsigned short indirect[2];
}inode;



typedef struct super
{
	unsigned char inode_bitmap[64];
	unsigned char data_bitmap[4031];
}super;

super superBlock;

void setMetadata(); //initialize metadata for first use of filesystem
inode get_inode(char*, inode); //given a file path and starting inode (directory), traverse directories to find inode
void write_to_file(inode); //write inode to file
inode read_from_file(int); //read data from file
char* read_from_inode(inode); //read from data section(s) corresponding with inode
char* get_buffer(inode); //given an inode, return its data section contents as string
char* read_super();

