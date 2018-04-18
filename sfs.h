#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>

typedef struct inode
{
	struct stat info; //see stat struct man page
	unsigned int index; //it's the same size either way
}inode;

typedef struct super
{
	unsigned char data_start; //start of data region
	unsigned short inode_count; //512 inodes
	unsigned short block_count; //16MB / 512B
	unsigned short block_size; //512B
	unsigned short freeBlocks;
	unsigned short inode_bmap;
	unsigned short data_bmap;
	static unsigned int system_size; //16MB
}super; 
