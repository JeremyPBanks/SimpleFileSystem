/*
  Simple File System

  This code is derived from function prototypes found /usr/include/fuse/fuse.h
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
  His code is licensed under the LGPLv2.

*/

#include "params.h"
#include "block.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <math.h>

#ifdef HAVE_SYS_XATTR_H
#include <sys/xattr.h>
#endif

#include "log.h"
#include "sfs.h"


/*--------GLOBALS----------*/
inode parentNode;
inode rootNode;
inode currentNode;
mode_t lastOpFlag; //holds file permission of just-opened file
mode_t lastDirOpFlag; //holds folder permission of just-opened directory
int fileFound;
/*-------------------------*/

///////////////////////////////////////////////////////////
//
// Prototypes for all these functions, and the C-style comments,
// come indirectly from /usr/include/fuse.h
//

/**
 * Initialize filesystem
 *
 * The return value will passed in the private_data field of
 * fuse_context to all file operations and as a parameter to the
 * destroy() method.
 *
 * Introduced in version 2.3
 * Changed in version 2.6
 */
void *sfs_init(struct fuse_conn_info *conn)
{
    disk_open("/.freespace/laf224/testfsfile");
    
    int bstat;
    char buffer[BLOCK_SIZE];

    //Read first byte(s) from file system. block_read() will return 0 if block is empty
    int readstat = block_read(0, buffer);
    log_msg("Readstat: %d\n",readstat);
    if(readstat == 0)
    {
	//initialize file system
	setMetadata();
	log_msg("Back from metadata init\n");
        //create root folder
	//char *rootData = "0&.\n";
	char rootData[BUFF_SIZE];
	strcpy(rootData, "8\t.\n");
	bstat = block_write(DATA_START, rootData);
	log_msg("Bstat after write: %d\n", bstat);

	bzero(rootData, BUFF_SIZE);
	bstat = block_read(DATA_START, rootData);
	log_msg("Bstat after read: %d\n", bstat);

	log_msg("Root data: %s\n", rootData);
    }

    else if(readstat < 0)
    {
	log_msg("Failed to initialize sfs.\n");
	exit(EXIT_FAILURE);
    }

    fprintf(stderr, "in bb-init\n");
    log_msg("\nsfs_init()\n");

    
    log_conn(conn);
    log_fuse_context(fuse_get_context());

    return SFS_DATA;
}

/**
 * Clean up filesystem
 *
 * Called on filesystem exit.
 *
 * Introduced in version 2.3
 */
void sfs_destroy(void *userdata)
{
    log_msg("\nsfs_destroy(userdata=0x%08x)\n", userdata);
    disk_close();
}

/** Get file attributes.
 *
 * Similar to stat().  The 'st_dev' and 'st_blksize' fields are
 * ignored.  The 'st_ino' field is ignored except if the 'use_ino'
 * mount option is given.
 */
int sfs_getattr(const char *path, struct stat *statbuf)
{
    int retstat = 0;
    //char fpath[BUFF_SIZE];
	char *fpath = (char*)malloc(strlen(path)+1);
    strcpy(fpath, path);
    log_msg("fpath: %s, path: %s\n",fpath,path);
    int myLen = strlen(fpath);

    if(strcmp(fpath,"/") == 0 && myLen == 1)
    {
	log_msg("JUST CHANGED / TO /.\n");
	strcpy(fpath, "/.");
    }
	inode whygod;
    inode start = get_inode("/", whygod, 0);

    inode myInode = get_inode(fpath, start,0);
    if(!fileFound)
    {
		free(fpath);
        return -2; //file not found
    }
    
    log_msg("myInode's number %d and its first block is index %d.\n", myInode.info.st_ino, myInode.direct[0]);

    char *grabbedInode = get_buffer(myInode);

    log_msg("Bruh, I just grabbed this inode: \n%s\n",grabbedInode);

    *statbuf = myInode.info;
    
    log_msg("\nsfs_getattr(path=\"%s\", statbuf=0x%08x)\n",
	  path, statbuf);
    
	free(grabbedInode);
	free(fpath);
    return retstat;
}

/**
 * Create and open a file
 *
 * If the file does not exist, first create it with the specified
 * mode, and then open it.
 *
 * If this method is not implemented or under Linux kernel
 * versions earlier than 2.6.15, the mknod() and open() methods
 * will be called instead.
 *
 * Introduced in version 2.5
 */
int sfs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    int i, j;
    int retstat = 0;
    log_msg("\nsfs_create(path=\"%s\", mode=0%03o, fi=0x%08x)\n",
	    path, mode, fi);

    char inodeMap[64];
    char dataMap[4031];
    char freeBit;
    int found = 0;

    char *pathCopy = (char*)malloc(strlen(path)+1);
	//char pathCopy[BUFF_SIZE];
    strcpy(pathCopy,path);

    //check if file exists
	inode dummy;
	inode start = get_inode("/",dummy, 0);
    inode fileNode = get_inode(pathCopy, start,0);

    if(!fileFound)//file doesn't exist
    {
	    //make fileNode into new inode
		log_msg("File does not exist\n");
	    char *superBuff = read_super();
	    memcpy(inodeMap, superBuff, 64);
	    memcpy(dataMap, (superBuff + 64), 4031);
	    char superCopyInode[64];
	    char superCopyData[4031];
	    memcpy(superCopyInode,inodeMap,64);
	    memcpy(superCopyData,dataMap,4031);
	    int bitLoc = -1;
	    int blockLoc = -1;

	    for(i = 0; i < 64; i++)
	    {
	        for(j = 7; j >= 0; j--)
	        {
		        freeBit = superCopyInode[i] & 0x1;
	            if(freeBit)
	            {
					if (i != 0 && j != 0)
					{
		            	bitLoc = j;
		            	blockLoc = i;
		            	break;
					}
	            }
		        superCopyInode[i] >>= 1;
	        }
	        if(bitLoc >= 0)
	        {
		        break;
	        }
	    }

		if (bitLoc < 0)//No space in inode region
		{
			return -ENOSPC;
		}

	    if (bitLoc == 0)
	    {
		    inodeMap[blockLoc] &= 0x7f;
	    }
	    else if (bitLoc == 1)
	    {
		    inodeMap[blockLoc] &= 0xbf;
	    }
	    else if (bitLoc == 2)
	    {
		    inodeMap[blockLoc] &= 0xdf;
	    }
	    else if (bitLoc == 3)
	    {
	        inodeMap[blockLoc] &= 0xef;
	    }
	    else if (bitLoc == 4)
	    {
            inodeMap[blockLoc] &= 0xf7;
	    }
	    else if (bitLoc == 5)
	    {
            inodeMap[blockLoc] &= 0xfb;
	    }
	    else if (bitLoc == 6)
	    {
            inodeMap[blockLoc] &= 0xfd;
	    }
	    else if (bitLoc == 7)
	    {
            inodeMap[blockLoc] &= 0xfe;
	    }

        int inodeBlock = (blockLoc*8) + bitLoc + INODE_START;
		//log_msg("[Create] blockLoc:%d,bitLoc:%d,inodeBlock:%d\n",blockLoc,bitLoc,inodeBlock);

        inode root_inode;
        root_inode.info.st_dev = 0;
        root_inode.info.st_ino = inodeBlock;
        root_inode.info.st_mode = S_IFREG | mode; //regular file with passed-in permissions
        root_inode.info.st_nlink = 1;
        root_inode.info.st_uid = getuid();
        root_inode.info.st_gid = getgid();
        root_inode.info.st_rdev = 0;
        root_inode.info.st_size = 0; //see string in init()
		root_inode.info.st_blksize = BLOCK_SIZE;
		root_inode.info.st_blocks = 0;
        

        for(i = 1; i < 32; i++)
        {
	    	root_inode.direct[i] = 0;
        }

        for(i = 0; i < 2; i++)
        {
            root_inode.indirect[i] = 0;
        }

        //get current time
        struct timespec time;
        clock_gettime(CLOCK_REALTIME, &time);
        root_inode.info.st_atime = time.tv_sec;
        root_inode.info.st_mtime = time.tv_sec;
        root_inode.info.st_ctime = time.tv_sec;

        root_inode.info.st_blksize = BLOCK_SIZE;
        root_inode.info.st_blocks = 1;


        int bitLocData = -1;
	    int blockLocData = -1;

	    for(i = 0; i < 4031; i++)
	    {
	        for(j = 7; j >= 0; j--)
	        {
		        freeBit = superCopyData[i] & 1;
	            if(freeBit)
	            {
					//if (j != 0 && i != 0)
					//{
				        bitLocData = j;
				        blockLocData = i;
				        break;
					//}
	            }
		        superCopyData[i] >>= 1;
	        }
	        if(bitLocData >= 0)
	        {
		        break;
	        }
	    }

		if (bitLocData < 0)//No space in inode region
		{
			return -ENOSPC;
		}

	    if (bitLocData == 0)
	    {
		    dataMap[blockLocData] &= 0x7f;
	    }
	    else if (bitLocData == 1)
	    {
		    dataMap[blockLocData] &= 0xbf;
	    }
	    else if (bitLocData == 2)
	    {
		    dataMap[blockLocData] &= 0xdf;
	    }
	    else if (bitLocData == 3)
	    {
	        dataMap[blockLocData] &= 0xef;
	    }
	    else if (bitLocData == 4)
	    {
            dataMap[blockLocData] &= 0xf7;
	    }
	    else if (bitLocData == 5)
	    {
            dataMap[blockLocData] &= 0xfb;
	    }
	    else if (bitLocData == 6)
	    {
            dataMap[blockLocData] &= 0xfd;
	    }
	    else if (bitLocData == 7)
	    {
            dataMap[blockLocData] &= 0xfe;
	    }

        int dataBlock = (blockLocData * 8) + (bitLocData + 520);
        
        root_inode.direct[0] = dataBlock;
		fi->flags = mode;
        write_to_file(root_inode);

        char insert_buffer[4095];
        memcpy(insert_buffer, inodeMap, 64);
        memcpy((insert_buffer + 64), dataMap, 4031);

        for(i = 0; i < 8; i++)
        {
            block_write(i, insert_buffer + (BLOCK_SIZE * i));
        }

		log_msg("Just updated bit maps\n");


        //update parent folder after insertion
		int myLen = strlen(pathCopy);
		if (pathCopy[myLen-1] == '/')
		{
			pathCopy[myLen-1] = '\0';
		}

		char *dummy=pathCopy;

		while(strstr(dummy,"/") != NULL)
		{
			dummy = strstr(dummy,"/") + 1;
		}

		log_msg("Path Copy, moment of truth: %s\n",dummy);
		char *directoryData;
		asprintf(&directoryData,"%d\t%s\n",root_inode.info.st_ino,dummy);
		fileFound = 1;
		writeToDirectory(directoryData, MY_APPEND);
		
		log_msg("Just updated directory data\n");
		free(directoryData);
		free(superBuff);
		log_msg("suoerBuff was freed\n");
    }

    else
    {
		//update file mode
		fileNode.info.st_mode = mode;
		write_to_file(fileNode);
    }
    
	free(pathCopy);

    return retstat;
}

/** Remove a file */
int sfs_unlink(const char *path)
{
    int retstat = 0;
    log_msg("sfs_unlink(path=\"%s\")\n", path);

	//char pathCopy[BUFF_SIZE];
	char *pathCopy = (char*)malloc(strlen(path)+1);
	strcpy(pathCopy,path);
	inode dummy;
	inode start = get_inode("/", dummy, 0);
	inode unlinkInode = get_inode(pathCopy,start,0);

	if(!fileFound)
	{
		return -ENOENT;
	}
	
	int i, len;
	
	if(unlinkInode.info.st_size % BLOCK_SIZE > 0)
    {
		len = (unlinkInode.info.st_size/BLOCK_SIZE)+1;
    }
    else
    {
		len = unlinkInode.info.st_size/BLOCK_SIZE;
    }

	char zeroBuff[BLOCK_SIZE];
	memset(zeroBuff,'\0',BLOCK_SIZE);
	int wstat;

	for(i = 0; i < len; i++)
	{
		if(i < 32)//direct pointers
		{
			wstat = block_write(unlinkInode.direct[i],zeroBuff);
			flipBit(unlinkInode.direct[i]);
			unlinkInode.direct[i] = 0;
		}

		else
		{
			//TODO: handle indirect pointers
		}
	}
	flipBit(unlinkInode.info.st_ino);
	unlinkInode.info.st_nlink = 0;
	writeToDirectory(pathCopy,MY_DELETE);
	//log_msg("[unlink] okay...now what?\n");
    return retstat;
}

/** File open operation
 *
 * No creation, or truncation flags (O_CREAT, O_EXCL, O_TRUNC)
 * will be passed to open().  Open should check if the operation
 * is permitted for the given flags.  Optionally open may also
 * return an arbitrary filehandle in the fuse_file_info structure,
 * which will be passed to all file operations.
 *
 * Changed in version 2.2
 */
int sfs_open(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;
	inode dummy;
    inode start = get_inode("/", dummy, 0);

    char *pathCopy = (char*)malloc(strlen(path)+1);

    strcpy(pathCopy,path);

    inode checkInode = get_inode(pathCopy,start,0);

    if (!fileFound)
    {
	    return -ENOENT; //file not found
    }

	//update last-accessed time
	struct timespec time;
	clock_gettime(CLOCK_REALTIME, &time);

    log_msg("\nsfs_open(path\"%s\", fi=0x%08x)\n",path, fi);

	checkInode.info.st_atime = time.tv_sec;
	write_to_file(checkInode);
	log_msg("Open Success.\n");

	if(checkInode.info.st_mode & S_IFMT != S_IFREG) //check if file is actually a regular file
	{
		return -EPERM; //operation not permitted (I guess)
	}
	
	lastOpFlag = checkInode.info.st_mode; //record file permissions
	//fi->flags = checkInode.info.st_mode;
	free(pathCopy);
	return retstat;    
}

/** Release an open file
 *
 * Release is called when there are no more references to an open
 * file: all file descriptors are closed and all memory mappings
 * are unmapped.
 *
 * For every open() call there will be exactly one release() call
 * with the same flags and file descriptor.  It is possible to
 * have a file opened more than once, in which case only the last
 * release will mean, that no more reads/writes will happen on the
 * file.  The return value of release is ignored.
 *
 * Changed in version 2.2
 */
int sfs_release(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;
    log_msg("\nsfs_release(path=\"%s\", fi=0x%08x)\n",
	  path, fi);
    
	//nothing to do here; we don't use any heap-allocated structs

    return retstat;
}

/** Read data from an open file
 *
 * Read should return exactly the number of bytes requested except
 * on EOF or error, otherwise the rest of the data will be
 * substituted with zeroes.  An exception to this is when the
 * 'direct_io' mount option is specified, in which case the return
 * value of the read system call will reflect the return value of
 * this operation.
 *
 * Changed in version 2.2
 */
int sfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    int retstat = 0;
    log_msg("\nsfs_read(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",path, buf, size, offset, fi);

	char *tempPath = malloc(strlen(path)+1);
	strcpy(tempPath, path);
	inode sup = get_inode(tempPath, rootNode, 0);
	free(tempPath);
	lastOpFlag = sup.info.st_mode;

	lastOpFlag &= S_IRUSR; //mask file mode to get read permissions

	if(lastOpFlag == S_IRUSR)
	{
		//log_msg("[Read] Permission Validated.\n");
	}

	else if((fi->flags & S_IRUSR) == S_IRUSR)
	{
		log_msg("[Read] Permission Validated by FUSE Struct.\n");
	}

	else
	{
		log_msg("[Read] Invalid Permission. Actual: %d OR %d Expected: %d\n", lastOpFlag, (fi->flags & S_IRUSR), S_IRUSR);
		return -EACCES; //permission denied
	}

	//char fPath[BUFF_SIZE];
	char *fPath = (char*)malloc(strlen(path)+1);
	strcpy(fPath, path);

	if(size <= 0) //if request to read 0 bytes or a null pointer is passed
	{
		return 0; //0 bytes read
	}

	if(buf == NULL)
	{
		log_msg("[Read] NULL Buffer\n");
		return -EFAULT;//Bad address
	}
	inode dummy;
	inode start = get_inode("/", dummy, 0);
	inode readNode = get_inode(fPath, start, 0);

	if(!fileFound)
	{
		free(fPath);
		//log_msg("[Read] File Not Found\n");
		return -ENOENT; //file not found
	}

	char *inodeString = get_buffer(readNode);
	char *stringStart = inodeString;

	inodeString += offset;
	int bytes;

	if((bytes = readNode.info.st_size - offset) >= size) //if file contains enough bytes to read number requested
	{
		retstat = size; //read all requested bytes
	}

	else
	{
		retstat = bytes; //read bytes up to end of file
	}


	memcpy(buf, inodeString, retstat); //TODO: according to man pages, no errors are defined for memcpy. Do we have to do any sanity checks here?
	//log_msg("[Read] Free Incoming\n");
	free(stringStart);
	//log_msg("[Read] Free Successful\n");

	//TODO:Document this
	/*if((offset+size)  >= readNode.info.st_size)//if starting point + #bytes to read goes past size of file
	{
		free(fPath);
		return -EOF; //end-of-file: return 0 (according to pdf)
	}*/
	
	free(fPath);
	//log_msg("[Read] returning:%d\n",retstat);
    return retstat; //restat for read/write contains number of bytes written/read in operation
}

/** Write data to an open file
 *
 * Write should return exactly the number of bytes requested
 * except on error.  An exception to this is when the 'direct_io'
 * mount option is specified (see read operation).
 *
 * Changed in version 2.2
 */
int sfs_write(const char *path, const char *buf, size_t size, off_t offset,
	     struct fuse_file_info *fi)
{
    int retstat = 0;
	int i;
    log_msg("\nsfs_write(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
	    path, buf, size, offset, fi);

	char *tempPath = malloc(strlen(path)+1);
	strcpy(tempPath, path);
	inode sup = get_inode(tempPath, rootNode, 0);
	free(tempPath);
	lastOpFlag = sup.info.st_mode;

	//log_msg("[Write] buff:%s\n",buf);


	lastOpFlag &= S_IWUSR; //mask file mode to get write permissions

	if(lastOpFlag == S_IWUSR)
	{
		//log_msg("[Write] Permission Validated.\n");
	}

	else if((fi->flags & S_IWUSR) == S_IWUSR)
	{
		//log_msg("[Write] Permission Validated by FUSE Struct.\n");
	}

	else
	{
		log_msg("[Write] Invalid Permission. Actual: %d OR %d Expected: %d\n", lastOpFlag, (fi->flags & S_IWUSR), S_IWUSR);
		return -EACCES; //permission denied
	}


	if(buf == NULL)
	{
		log_msg("[Write] NULL Buffer\n");
		return -EFAULT;
	}

	//char fPath[BUFF_SIZE];
	char *fPath = (char*)malloc(strlen(path)+1);
	strcpy(fPath, path);
	inode dummy;
	inode start = get_inode("/", dummy, 0);
	inode writeNode = get_inode(fPath, start, 0);
	if(!fileFound)
	{
		free(fPath);
		//log_msg("[Write] File Not Found\n");
		return -ENOENT; //file not found
	}

	char *inodeString = get_buffer(writeNode);
	inodeString = (char*)realloc(inodeString, writeNode.info.st_size + offset + size);

	char *startString = inodeString;
	inodeString += offset;

	//log_msg("[Write] Now memcopying...\n");
	memcpy(inodeString, buf, size);	
	
	//log_msg("[Write] Now writing...\n");
	retstat = loopWrite(startString, &writeNode);
	//log_msg("[Write] ...File Written\n");

	//update inode modification time & size
	struct timespec time;
	clock_gettime(CLOCK_REALTIME, &time); 
	writeNode.info.st_mtime = time.tv_sec;

	//TODO: will we ever have to worry about file size decreasing?
	if(retstat > 0)
	{
		int fSize = writeNode.info.st_size - offset; //get number of bytes remaining after offset

		if(size > fSize)//if more bytes are written to file than current size of file, the file will expand in size
		{
			writeNode.info.st_size += (size - fSize); //increase file size by spillover bytes
			int len;
			if(writeNode.info.st_size % BLOCK_SIZE > 0)
    		{
				len = (writeNode.info.st_size/BLOCK_SIZE)+1;
    		}
    		else
    		{
				len = writeNode.info.st_size/BLOCK_SIZE;
    		}
			writeNode.info.st_blocks = len;
		}

		else
		{
			//part of file was overwritten, but the file has not grown. Nothing changes here
		}
		//log_msg("[Write] size:%d,offset:%d\n",writeNode.info.st_size,offset);
	}

	write_to_file(writeNode);

	//log_msg("[Write] Free Incoming\n");
	free(startString);
	free(fPath);
	//log_msg("[Write] Free Successful\n");
    return retstat; //restat for read/write contains number of bytes written/read in operation
}


/** Create a directory */
int sfs_mkdir(const char *path, mode_t mode)
{
    int retstat = 0;
    log_msg("\nsfs_mkdir(path=\"%s\", mode=0%3o)\n",
	    path, mode);

	int i;

	char *fPath = (char*)malloc(strlen(path)+1);
	strcpy(fPath, path);

	inode dummy;
	inode start = get_inode("/", dummy, 0);
	inode dirNode = get_inode(fPath,start,0);

	if(!fileFound)
	{
		char *pathStart = fPath;
		while(strstr(fPath, "/") != NULL)
		{
			fPath = strstr(fPath, "/")+1;//get name of folder to be created
		}

		//set bitmaps and return index of free block (returns negative on error)
		int nodeIndex = myInodeIndex(); 
		int blockIndex = myBlockIndex();
		if(nodeIndex < 0 || blockIndex < 0)
		{
			//no room in file system
			return -ENOSPC;
		}

		char *currentBuffer, *parentBuffer;
		asprintf(&parentBuffer, "%d\t%s\n", nodeIndex, fPath);
		asprintf(&currentBuffer, "%d\t.\n", nodeIndex);

		dirNode.info.st_dev = 0;
    	dirNode.info.st_ino = nodeIndex;
    	dirNode.info.st_mode = S_IFDIR | mode; //directory with passed-in permissions
    	dirNode.info.st_nlink = 1;
    	dirNode.info.st_uid = getuid();
    	dirNode.info.st_gid = getgid();
    	dirNode.info.st_rdev = 0;
    	dirNode.info.st_size = strlen(currentBuffer)+1;
		//log_msg("[mkdir] Directory size: %d\n", dirNode.info.st_size);
		dirNode.info.st_blksize = BLOCK_SIZE;
		dirNode.info.st_blocks = 1;
		dirNode.direct[0] = blockIndex;

		for(i = 1; i < 32; i++)
        {
	    	dirNode.direct[i] = 0;
        }

        for(i = 0; i < 2; i++)
        {
            dirNode.indirect[i] = 0;
        }

		struct timespec time;
        clock_gettime(CLOCK_REALTIME, &time);
        dirNode.info.st_atime = time.tv_sec;
        dirNode.info.st_mtime = time.tv_sec;
        dirNode.info.st_ctime = time.tv_sec;

		fileFound = 1;

		//log_msg("[mkdir] About to write to parent directory: %s\n", parentBuffer);	
		writeToDirectory(parentBuffer, MY_APPEND); //update parent directory

		//log_msg("[mkdir] About to write new directory metadata\n");
		write_to_file(dirNode); //write new inode to metadata region

		//log_msg("[mkdir] About to write to this directory: %s\n", currentBuffer);
		parentNode = dirNode; //set parent directory to self to add dirNode to its own directory
		writeToDirectory(currentBuffer, MY_APPEND); //update directory of self

		//log_msg("[mkdir] Returning from mkdir\n");
		free(parentBuffer); free(currentBuffer); free(pathStart);
	}    


    
    return retstat;
}


/** Remove a directory */
int sfs_rmdir(const char *path)
{
    int retstat = 0;
	int rmBlocks;
    log_msg("sfs_rmdir(path=\"%s\")\n",path);

	char *fPath = (char*)malloc(strlen(path)+1);
	strcpy(fPath, path);
	//char *fCopy = fPath;

	inode dummy;
	inode start = get_inode("/", dummy, 0);

	if(!fileFound)
	{
		//log_msg("[sfs_rmdir] File not found?\n");
    	return -ENOENT;//file not found
	}    

	//log_msg("[sfs_rmdir] Removing nested stuff...\n");
	removeSubDir(fPath,start);
	//log_msg("[sfs_rmdir] Just removed nested stuff...\n");

	inode dirNode = get_inode(fPath,start,0);

	if(dirNode.info.st_size % BLOCK_SIZE > 0)
    {
		rmBlocks = (dirNode.info.st_size/BLOCK_SIZE)+1;
    }
    else
    {
		rmBlocks = dirNode.info.st_size/BLOCK_SIZE;
    }
  
	//log_msg("[sfs_rmdir] Flipping bits...\n");
	flipBit(dirNode.info.st_ino);
	int i;
	for(i=0;i<rmBlocks;i++)
	{
		flipBit(dirNode.direct[i]);
	}

	//log_msg("[sfs_rmdir] Finalizing in writeToDirectory...\n");
	writeToDirectory(fPath, MY_DELETE);

	//log_msg("[sfs_rmdir] Just finalized in writeToDirectory...\n");
	free(fPath);
	//log_msg("[sfs_rmdir] Just removed the directory\n");
    return retstat;
}


/** Open directory
 *
 * This method should check if the open operation is permitted for
 * this  directory
 *
 * Introduced in version 2.3
 */
int sfs_opendir(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;
    log_msg("\nsfs_opendir(path=\"%s\", fi=0x%08x)\n",
	  path, fi);

	char *fPath = (char*)malloc(strlen(path)+1);
	strcpy(fPath, path);

	inode dummy;
	inode start = get_inode("/", dummy, 0);
	inode dirNode = get_inode(fPath,start,0);

	if(!fileFound)
	{
    	return -ENOENT;//file not found
	}    

	lastDirOpFlag = dirNode.info.st_mode;
	fi->flags = dirNode.info.st_mode;

	free(fPath);
    return retstat;
}

/** Read directory
 *
 * This supersedes the old getdir() interface.  New applications
 * should use this.
 *
 * The filesystem may choose between two modes of operation:
 *
 * 1) The readdir implementation ignores the offset parameter, and
 * passes zero to the filler function's offset.  The filler
 * function will not return '1' (unless an error happens), so the
 * whole directory is read in a single readdir operation.  This
 * works just like the old getdir() method.
 *
 * 2) The readdir implementation keeps track of the offsets of the
 * directory entries.  It uses the offset parameter and always
 * passes non-zero offset to the filler function.  When the buffer
 * is full (or an error happens) the filler function will return
 * '1'.
 *
 * Introduced in version 2.3
 */
int sfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
	       struct fuse_file_info *fi)
{
	//check directory permissions

	log_msg("-----sfs_readdir-----\n");
    int retstat = 0;
	struct stat fillMe;
	int pathLen = strlen(path);
	char myPathCopy[pathLen+1];
	strcpy(myPathCopy,path);
    log_msg("after strcpy, wtf is mypathcopy %s and path %s god wtf\n", myPathCopy, path);
    inode dummy;
	inode start = get_inode("/", dummy, 0);
	inode myInode = get_inode(myPathCopy,start,0);
	log_msg("after get inode\n");
	if (!fileFound)
	{
		return -ENOENT;
	}

	//TODO: check read permission

	char* inodeAsString = get_buffer(myInode);
    log_msg("after get buffer\n");
	if (inodeAsString == NULL)
	{
		return retstat;
	}
	char* myFree = inodeAsString;
	char* token = strtok(inodeAsString,"\n");
	log_msg("after first strtok\n");
	while(token != NULL)
	{
		char* myName=strstr(token,"\t")+1;
		if(filler(buf,myName,&fillMe,0) != 0)
		{
			free(myFree);
			return -ENOMEM;
		}
		token = strtok(NULL,"\n");
    }
    log_msg("after while loop\n");
	free(myFree);
	log_msg("after freeing\n");
    return retstat;
}

/** Release directory
 *
 * Introduced in version 2.3
 */
int sfs_releasedir(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;

	//nothing to do here; no dynamically allocated structs    

    return retstat;
}

struct fuse_operations sfs_oper = {
  .init = sfs_init,
  .destroy = sfs_destroy,

  .getattr = sfs_getattr,
  .create = sfs_create,
  .unlink = sfs_unlink,
  .open = sfs_open,
  .release = sfs_release,
  .read = sfs_read,
  .write = sfs_write,

  .rmdir = sfs_rmdir,
  .mkdir = sfs_mkdir,

  .opendir = sfs_opendir,
  .readdir = sfs_readdir,
  .releasedir = sfs_releasedir
};

void sfs_usage()
{
    fprintf(stderr, "usage:  sfs [FUSE and mount options] diskFile mountPoint\n");
    abort();
}

int main(int argc, char *argv[])
{

    int fuse_stat;
    struct sfs_state *sfs_data;
    
    // sanity checking on the command line
    if ((argc < 3) || (argv[argc-2][0] == '-') || (argv[argc-1][0] == '-'))
	sfs_usage();

    sfs_data = malloc(sizeof(struct sfs_state));
    if (sfs_data == NULL) {
	perror("main calloc");
	abort();
    }

    // Pull the diskfile and save it in internal data
    sfs_data->diskfile = argv[argc-2];
    argv[argc-2] = argv[argc-1];
    argv[argc-1] = NULL;
    argc--;
    
    sfs_data->logfile = log_open();
    
    // turn over control to fuse
    fprintf(stderr, "about to call fuse_main, %s \n", sfs_data->diskfile);
    fuse_stat = fuse_main(argc, argv, &sfs_oper, sfs_data);
    fprintf(stderr, "fuse_main returned %d\n", fuse_stat);
    
    return fuse_stat;
}

void setMetadata()
{
	char superBuffer[4095];

    superBuffer[0] = 0x7f;

	int i;
	for(i = 1; i < 4094; i++)
	{
		superBuffer[i] = 0xff;
	}

	superBuffer[64] = 0x7f;

	int stat;
	char readbuff[BLOCK_SIZE];
	
	for(i = 0; i < 8; i++)
	{
		stat = block_write(i, (superBuffer + (i * BLOCK_SIZE)));
		//log_msg("[setMetadata] Wrote super to block %d.\tStatus: %d\n", i, stat);
		stat = block_read(i, readbuff);
		//log_msg("[setMetadata] Read super section from file: %s.\tStatus: %d\n", readbuff, stat);
		bzero(readbuff, BLOCK_SIZE);
	}


    //fill in root inode
    inode root_inode;
    root_inode.info.st_dev = 0;
    root_inode.info.st_ino = INODE_START;
    root_inode.info.st_mode = S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO; //give root EVERYTHING
    root_inode.info.st_nlink = 1;
    root_inode.info.st_uid = getuid();
    root_inode.info.st_gid = getgid();
    root_inode.info.st_rdev = 0;
    root_inode.info.st_size = 5; //see string in init()
	root_inode.info.st_blksize = BLOCK_SIZE;
	root_inode.info.st_blocks = 1;
    root_inode.direct[0] = DATA_START;

    for(i = 1; i < 32; i++)
    {
	root_inode.direct[i] = 0;
    }

    for(i = 0; i < 2; i++)
    {
        root_inode.indirect[i] = 0;
    }

    //get current time
    struct timespec time;
    clock_gettime(CLOCK_REALTIME, &time);
    root_inode.info.st_atime = time.tv_sec;
    root_inode.info.st_mtime = time.tv_sec;
    root_inode.info.st_ctime = time.tv_sec;

    root_inode.info.st_blksize = BLOCK_SIZE;
    root_inode.info.st_blocks = 1;

    write_to_file(root_inode);
    rootNode = root_inode;
    currentNode = root_inode;
}

inode get_inode(char *path, inode this_inode,int depth)
{
    //TODO: L: I'll make this recursive for now, we'll see if we can change that later
    inode tgt;

    //log_msg("~~[get_inode]~~\n");

    fileFound = 1;

    //log_msg("[get_inode] Path in get_inode: %s\n", path);
	if(strcmp(path, "/") == 0)
    {
		rootNode = read_from_file(8);
		parentNode = rootNode;
    	return rootNode;
    }
    if(strstr(path, ROOT_PATH) != NULL)//if root path exists in parameter
    {
		//log_msg("[get_inode] Chris: It'll never reach here!\n");
		path += strlen(ROOT_PATH);
    }

    //remove leading and trailing slashes
    if(path[0] == '/')
    {
		path++;
    }

    if(path[strlen(path)-1] == '/')
    {
		path[strlen(path)-1] = '\0';
    }

    //base case
    if(strstr(path, "/") == NULL)
    {
		//log_msg("[get_inode] Searching for file '%s'.\n", path);
		//TODO: read directory here, get inode

		char *bufferTgt = get_buffer(this_inode);
		char *buffTgtStart = bufferTgt;
		//log_msg("[get_inode] get_buffer just returned:%s\n",bufferTgt);
        //parse string to find filename
        char *tokenTgt = strtok(bufferTgt, "\n");
        char *fnameTgt;
        char inumTgt[4]; //max size
        int tgtNodeTgt;
        int stroffsetTgt;

        while(tokenTgt != NULL)
        {
	    //need to manually tokenize here; can't re-call strtok on new string (STATEFUL)
	        fnameTgt = strstr(tokenTgt, "\t")+1;
	        if(strcmp(path, fnameTgt) == 0)//file found
	        {
		        //log_msg("[get_inode] fnameTgt: %s",fnameTgt);
	            stroffsetTgt = fnameTgt - tokenTgt - 1;
	            tokenTgt[stroffsetTgt] = '\0';
	            strcpy(inumTgt, tokenTgt);
	            tgtNodeTgt = atoi(inumTgt);
	            tgt = read_from_file(tgtNodeTgt);
				//log_msg("[get_inode] just read from file\n");
	            break;
	        }
	        tokenTgt = strtok(NULL, "\n");
        }

        if (tokenTgt == NULL)
        {
            //log_msg("[get_inode] Failed to find inode with path: %s\n",path);
	        fileFound = 0;
        }

		//log_msg("[get_inode] freeing buffer...\n");
        free(buffTgtStart);
		//log_msg("[get_inode] freed buffer...\n");

		if (depth == 0)
		{
			rootNode = read_from_file(8);
			parentNode = rootNode;
		}

		//log_msg("~~End of [get_inode]~~\n");

	    return tgt;
    }

    /*INODE-SEARCH STEPS*/
    /*
	-read path up to first slash
	-search current directory (given by root_inode) for filename
	-translate filename to inode
	-'increment' path string to next slash (use strstr?)
	-[FOR NOW] recurse with new path & root_inode
    */

    char *newpath = strstr(path, "/") + 1;
    char filename[BUFF_SIZE];
    int i = newpath - path - 1;
    memcpy(filename, path, i);
    filename[i] = '\0';
    
    //log_msg("[get_inode] Searching for folder '%s' [name size %d] in path '%s'.\n", filename, i, path);

    char *buffer = get_buffer(this_inode);

    //parse string to find filename
    char *token = strtok(buffer, "\n");
    char *fname;
    char inum[4]; //max size
    int tgtNode;
    int stroffset;

    while(token != NULL)
    {
	//need to manually tokenize here; can't re-call strtok on new string (STATEFUL)
	fname = strstr(token, "\t")+1;
	if(strcmp(filename, fname) == 0)//file found
	{
	    stroffset = fname - token - 1;
	    token[stroffset] = '\0';
	    strcpy(inum, token);
	    tgtNode = atoi(inum);
	    tgt = read_from_file(tgtNode);
	    break;
	}
	token = strtok(NULL, "\n");
    }

    if (token == NULL)
    {
        log_msg("Failed to find inode with path: %s\n",path);
	fileFound = 0;
	return tgt;
    }

    free(buffer);
    log_msg("[get_inode] Newpath is %s\n and tgt is %i", newpath, tgt.info.st_ino);
	parentNode = tgt;
	//log_msg("~~End of [get_inode]~~\n");
    return get_inode(newpath, tgt, depth+1);
}

void write_to_file(inode insert_inode)
{
    char *rootString;

    asprintf(&rootString, "%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t", insert_inode.info.st_dev, insert_inode.info.st_ino, insert_inode.info.st_mode, insert_inode.info.st_nlink, insert_inode.info.st_uid, insert_inode.info.st_gid, insert_inode.info.st_rdev, insert_inode.info.st_size, insert_inode.direct[0], insert_inode.direct[1], insert_inode.direct[2], insert_inode.direct[3], insert_inode.direct[4], insert_inode.direct[5], insert_inode.direct[6], insert_inode.direct[7], insert_inode.direct[8], insert_inode.direct[9], insert_inode.direct[10], insert_inode.direct[11], insert_inode.direct[12],insert_inode.direct[13],insert_inode.direct[14],insert_inode.direct[15],insert_inode.direct[16],insert_inode.direct[17],insert_inode.direct[18],insert_inode.direct[19],insert_inode.direct[20],insert_inode.direct[21],insert_inode.direct[22],insert_inode.direct[23],insert_inode.direct[24],insert_inode.direct[25],insert_inode.direct[26],insert_inode.direct[27],insert_inode.direct[28],insert_inode.direct[29],insert_inode.direct[30],insert_inode.direct[31],insert_inode.indirect[0], insert_inode.indirect[1], insert_inode.info.st_atime, insert_inode.info.st_mtime, insert_inode.info.st_ctime, insert_inode.info.st_blksize, insert_inode.info.st_blocks);

    int bstat = block_write(insert_inode.info.st_ino , rootString);
    
    if(bstat < 0)
    {
		log_msg("Failed to write inode %d to file.\n", insert_inode.info.st_ino);
    }

    free(rootString);
}


inode read_from_file(int node)
{
    inode testnode;
    int i;

    char buf[BUFF_SIZE];
    int bstat = block_read(node, buf);
    if(bstat < 0)
    {
		log_msg("Failed to read from node %d.\n", node);
    }

    char *token = strtok(buf, "\t");

    testnode.info.st_dev = atoi(token);
    token = strtok(NULL, "\t");
    testnode.info.st_ino = atoi(token);
    token = strtok(NULL, "\t");
    testnode.info.st_mode = atoi(token);
    token = strtok(NULL, "\t");
    testnode.info.st_nlink = atoi(token);
    token = strtok(NULL, "\t");
    testnode.info.st_uid = atoi(token);
    token = strtok(NULL, "\t");
    testnode.info.st_gid = atoi(token);
    token = strtok(NULL, "\t");
    testnode.info.st_rdev = atoi(token);
    token = strtok(NULL, "\t");
    testnode.info.st_size = atoi(token);
    token = strtok(NULL, "\t");

	int len;
	if(testnode.info.st_size % BLOCK_SIZE > 0)
    {
		len = (testnode.info.st_size/BLOCK_SIZE)+1;
    }
    else
    {
		len = testnode.info.st_size/BLOCK_SIZE;
    }
	testnode.info.st_blocks = len;

    for(i = 0; i < 32; i++)
    {
		testnode.direct[i] = atoi(token);
		token = strtok(NULL, "\t");
    }

    //log_msg("Indirect Time!\n");
    testnode.indirect[0] = atoi(token);
    token = strtok(NULL, "\t");
    testnode.indirect[1] = atoi(token);
    token = strtok(NULL, "\t");

    testnode.info.st_atime = atoi(token);
    token = strtok(NULL, "\t");
    testnode.info.st_mtime = atoi(token);
    token = strtok(NULL, "\t");
    testnode.info.st_ctime = atoi(token);
    token = strtok(NULL, "\t");
    testnode.info.st_blksize = atoi(token);
    token = strtok(NULL, "\t");
    testnode.info.st_blocks = atoi(token);
    
    //log_msg("[read_from_file] Just tokenized!\n");
    

    return testnode;
}

//NOTE: this function returns an allocated string which must be freed
char* get_buffer(inode node)
{
    if(!fileFound)
    {
		//log_msg("[get_buffer] fileFound sucks\n");
		return NULL;
    }

    int i;
    char *buffer = NULL;
    char readbuff[BLOCK_SIZE];

    int len;
    int bstat;

    //implementing a ceil()
	

    if(node.info.st_size % BLOCK_SIZE > 0)
    {
		len = (node.info.st_size/BLOCK_SIZE)+1;
    }
    else
    {
		len = node.info.st_size/BLOCK_SIZE;
    }

	//log_msg("[get_buffer] len:%d,size:%d\n",len,node.info.st_size);

    //TODO: for now, assume directories won't require indirect inodes; total string size less than 32 * 512. (Maybe) fix this later
    for(i = 0; i < len; i++)
    {
		bstat = block_read(node.direct[i], readbuff);
		//log_msg("[get_buffer] status of block_read from data block %d: %d\n", node.direct[i], bstat);
		int count = 0;
		char*newLinePtr = strstr(readbuff, "\n");
		while(newLinePtr != NULL)
		{
			newLinePtr += 1;
			count++;
			newLinePtr = strstr(newLinePtr, "\n");
		}
        char *nullPtr = readbuff + strlen(readbuff) + 1;
		if(nullPtr != NULL)
		{
			//nullPtr += 1;
			//log_msg("[get_buffer] nullPtrString:%s\n",nullPtr);

		}
		//log_msg("[get_buffer] Newline count:%d\n",count);
		//log_msg("[get_buffer] Yo, this is readbuff:\n%s\n",readbuff);
		if(bstat < 0)
		{
			//log_msg("[get_buffer] Failed to read block in get_buffer.\n");
		}

		if((i+1) == len)
		{
			int thislen = strlen(readbuff)+1;
			//log_msg("[get_buffer] bytes in last block: %d\n",thislen);
		}

		buffer = realloc(buffer, BLOCK_SIZE * (i+1));
		memcpy((buffer + (BLOCK_SIZE * i)), readbuff, BLOCK_SIZE);
		memset(readbuff, '\0', BLOCK_SIZE);
    }

	//log_msg("[get_buffer] Returning: [begin]\n%s\n[end]", buffer);
    return buffer;
}


char* read_super()
{
    char *buffer = (char*)malloc(BLOCK_SIZE * 8);
    char readbuff[BLOCK_SIZE];
    int i, bstat;

    for(i = 0; i < 8; i++)
    {
		bstat = block_read(i, readbuff);
		memcpy((buffer + (BLOCK_SIZE * i)), readbuff, BLOCK_SIZE);
		bzero(readbuff, BLOCK_SIZE);
    }

    return buffer;
}

void writeToDirectory(char *fPath, int flag) //1 = append, 0 = remove
{
	log_msg("In writeToDirectory\n");
	log_msg("Parent Ino:%d\n",parentNode.info.st_ino);
	char *inodeString = get_buffer(parentNode);
	if(inodeString == NULL)
	{
		//log_msg("[writeToDirectory] Writing to empty directory\n"); 
		inodeString = (char*)malloc(1);
		inodeString[0] = '\0';
	}

	log_msg("Directory String:\n%s\n",inodeString);

	if(flag == 1)//append
	{
		int iLen = strlen(inodeString);
		int fLen = strlen(fPath);
		log_msg("inode string size: %d fpath size:%d\n",iLen,fLen);
		inodeString = realloc(inodeString,iLen + fLen + 1);
		strcat(inodeString,fPath);
	}

	else if(flag == 0)//remove
	{
		//log_msg("[writeToDirectory] Deleting in writeToDirectory\n");
		inode dummy;
		inode start = get_inode("/",dummy, 0);
		inode removalNode = get_inode(fPath, start, 0);
		if(!fileFound)
		{
			//log_msg("[writeToDirectory] Failed to find file when deleting\n");
			return;
		}

		while(strstr(fPath, "/") != NULL)
		{
			fPath = strstr(fPath, "/")+1;//get name of file to be removed
		}


		char *format, *tempBuff;
		//log_msg("[writeToDirectory] Ino: %d FileName: %s\n",removalNode.info.st_ino, fPath);

		asprintf(&format, "%d\t%s\n", removalNode.info.st_ino, fPath);//create 'format' of string to search for: Inode Number [tab] Filename
		
		char *tgtString = strstr(inodeString, format);//get location of string to be removed. 
		
		int strLen = strstr(tgtString, "\n") - tgtString + 1;//get length of string about to be removed

		asprintf(&tempBuff, "%s", tgtString + strLen);//copy all bytes directly after string about to be removed

		memset((inodeString + (tgtString - inodeString)), '\0', strlen(tgtString));//clear out entire string starting from removal target

		strcat(inodeString, tempBuff);//re-insert copied string
	
		free(tempBuff);
		free(format);
	}

	else //sanity check
	{
		//log_msg("[writeToDirectory] Holy shit my sanity is gone (check writeToDirectory)\n");
	}

	//char myStringArg[BUFF_SIZE];
	int xyz = strlen(inodeString)+1;
	char *myStringArg = (char*)malloc(xyz);
	int count = 0;
	char *newLinePtr;
	newLinePtr = strstr(inodeString, "\n");
	while(newLinePtr != NULL)
	{
		newLinePtr += 1;
		count++;
		newLinePtr = strstr(newLinePtr, "\n");
	}
	//log_msg("[writeToDirectory] inodeString+1: %d, number of newline characters in inode string before strcpy: %d\n",xyz, count);
	strcpy(myStringArg,inodeString);
	parentNode.info.st_size = strlen(myStringArg) + 1;

	int len;
	if(parentNode.info.st_size % BLOCK_SIZE > 0)
    {
		len = (parentNode.info.st_size/BLOCK_SIZE)+1;
    }
    else
    {
		len = parentNode.info.st_size/BLOCK_SIZE;
    }
	parentNode.info.st_blocks = len;

	//log_msg("[writeToDirectory] String: %s File Size: %d Block Count: %d\n", myStringArg, parentNode.info.st_size, len);

	loopWrite(myStringArg, &parentNode);
	write_to_file(parentNode);
	//log_msg("[writeToDirectory] New Directory Contents:\n%s\n",myStringArg);
	count = 0;
	newLinePtr = strstr(myStringArg, "\n");
	while(newLinePtr != NULL)
	{
		newLinePtr += 1;
		count++;
		newLinePtr = strstr(newLinePtr, "\n");
	}
	//log_msg("[writeToDirectory] number of newline characters in inode string after strcpy: %d\n", count);
	free(inodeString);
	free(myStringArg);
	rootNode = read_from_file(8);
}

int loopWrite(char* myString, inode* thisNode)
{
	inode node = *thisNode;
	//log_msg("[loopWrite] In loopWrite\n");
	int bstat;
	int mySize = strlen(myString)+1;
	//log_msg("[loopWrite] myString: %s\tmySize:%d\n",myString,mySize);
	int myBlockCount;
	int totalWritten = 0;
	int i,thisBlock,thisInode;
	char myZero[BLOCK_SIZE];
	memset(myZero,'\0',BLOCK_SIZE);
	char writeBuff[513];
	writeBuff[512] = '\0';
	char *writeStart;


	//implementing a ceil()
    if(mySize % BLOCK_SIZE > 0)
    {
		myBlockCount = (mySize/BLOCK_SIZE)+1;
    }
    else
    {
		myBlockCount = mySize/BLOCK_SIZE;
    }

	//log_msg("[loopWrite] myBlockCount-->%d\n",myBlockCount);

	for(i = 0; i < myBlockCount; i++)
	{
		if(i < 32)//use direct pointers
		{
			if(node.direct[i] == 0)//uninitialized direct pointer
			{
				//initialize block pointer
				//log_msg("[loopWrite] index check\n");
				thisBlock = myBlockIndex();
				//log_msg("[loopWrite] thisBlock----->%d\n",thisBlock);

				if (thisBlock < 0)//Out of space
				{
					//log_msg("[loopWrite] Out of Space {Direct}\n");
					*thisNode = node;
					return totalWritten;
				}
			
				node.direct[i] = thisBlock;
			}
			//log_msg("[loopWrite] Writing...\n");
			bstat = block_write(node.direct[i],myZero);//Clearing out just in case
			if(bstat < 1)
			{
				//log_msg("[loopWrite] Something in clearing out datablock, bstat:%d\n",bstat);
			}

			memcpy(writeBuff, myString, BLOCK_SIZE);
			if((writeStart = strstr(writeBuff, "\0")) - writeBuff < BLOCK_SIZE && writeStart - writeBuff > 0)
			{
				memset(writeStart, '\0', BLOCK_SIZE - (writeStart - writeBuff));
			}


			bstat = block_write(node.direct[i],writeBuff);//actual write
			memset(writeBuff, '\0', BLOCK_SIZE);

			if(bstat < 1)
			{
				//log_msg("[loopWrite] Something in actual data write, bstat:%d\n",bstat);
			}
		}
		else
		{
			if (node.indirect[0] == 0)//uninitialized indirect pointer
			{
				thisInode = myInodeIndex();
				log_msg("New Inode Index: %d\n",thisInode);
				if (thisInode < 0)//Out of space
				{
					//log_msg("[loopWrite] Out of Space {Indirect}\n");
					*thisNode = node;
					return totalWritten;
				}
				node.indirect[0] = thisInode;
				//loopWrite()
			}
			else
			{
			}
		}
		totalWritten += BLOCK_SIZE;
		//TODO:Possible seg fault; double check later
		//log_msg("[loopWrite] Next block...please don't seg fault\n");
		myString += BLOCK_SIZE;
	}

	//log_msg("[loopWrite] leaving loopwrite\n");
	*thisNode = node;
	return mySize;
}

int myBlockIndex()
{
		//log_msg("In myBlockIndex\n");
		char *superBuff = read_super();
		char inodeMap[64];
		char dataMap[4031];
	    memcpy(inodeMap, superBuff, 64);
	    memcpy(dataMap, (superBuff + 64), 4031);
	    //char superCopyInode[64];
	    char superCopyData[4031];
	    //memcpy(superCopyInode,inodeMap,64);
	    memcpy(superCopyData,dataMap,4031);
		int bitLocData = -1;
	    int blockLocData = -1;
		int i, j;
		char freeBit;

	    for(i = 0; i < 4031; i++)
	    {
	        for(j = 7; j >= 0; j--)
	        {
		        freeBit = superCopyData[i] & 1;
	            if(freeBit)
	            {
					bitLocData = j;
				    blockLocData = i;
				    break;
				}
		        superCopyData[i] >>= 1;
	        }
	        if(bitLocData >= 0)
	        {
		        break;
	        }
	    }

		if (bitLocData < 0)//Out of space
		{
			return -1;
		}

	    if (bitLocData == 0)
	    {
		    dataMap[blockLocData] &= 0x7f;
	    }
	    else if (bitLocData == 1)
	    {
		    dataMap[blockLocData] &= 0xbf;
	    }
	    else if (bitLocData == 2)
	    {
		    dataMap[blockLocData] &= 0xdf;
	    }
	    else if (bitLocData == 3)
	    {
	        dataMap[blockLocData] &= 0xef;
	    }
	    else if (bitLocData == 4)
	    {
            dataMap[blockLocData] &= 0xf7;
	    }
	    else if (bitLocData == 5)
	    {
            dataMap[blockLocData] &= 0xfb;
	    }
	    else if (bitLocData == 6)
	    {
            dataMap[blockLocData] &= 0xfd;
	    }
	    else if (bitLocData == 7)
	    {
            dataMap[blockLocData] &= 0xfe;
	    }

        int dataBlock = (blockLocData * 8) + (bitLocData + DATA_START);

        char insert_buffer[4095];
        memcpy(insert_buffer, inodeMap, 64);
        memcpy((insert_buffer + 64), dataMap, 4031);

        for(i = 0; i < 8; i++)
        {
            block_write(i, insert_buffer + (BLOCK_SIZE * i));
        }

		free(superBuff);
		return dataBlock;
}

int myInodeIndex()
{
		//log_msg("[myInodeIndex] In myInodeIndex\n");
		char *superBuff = read_super();
		char inodeMap[64];
		char dataMap[4031];
	    memcpy(inodeMap, superBuff, 64);
	    memcpy(dataMap, (superBuff + 64), 4031);
	    char superCopyInode[64];
	    //char superCopyData[64];
	    memcpy(superCopyInode,inodeMap,64);
	    //memcpy(superCopyData,dataMap,4031);
	    int bitLoc = -1;
	    int blockLoc = -1;
		int j,i;
		char freeBit;

	    for(i = 0; i < 64; i++)
	    {
			log_msg("Value of block: %x\n",superCopyInode[i] & 0xff);
	        for(j = 7; j >= 0; j--)
	        {
		        freeBit = superCopyInode[i] & 1;
	            if(freeBit)
	            {
		            bitLoc = j;
		            blockLoc = i;
		            break;
	            }
		        superCopyInode[i] >>= 1;
	        }
	        if(bitLoc >= 0)
	        {
		        break;
	        }
	    }

		if (bitLoc < 0)//Out of space
		{
			return -1;
		}

	    if (bitLoc == 0)
	    {
		    inodeMap[blockLoc] &= 0x7f;
	    }
	    else if (bitLoc == 1)
	    {
		    inodeMap[blockLoc] &= 0xbf;
	    }
	    else if (bitLoc == 2)
	    {
		    inodeMap[blockLoc] &= 0xdf;
	    }
	    else if (bitLoc == 3)
	    {
	        inodeMap[blockLoc] &= 0xef;
	    }
	    else if (bitLoc == 4)
	    {
            inodeMap[blockLoc] &= 0xf7;
	    }
	    else if (bitLoc == 5)
	    {
            inodeMap[blockLoc] &= 0xfb;
	    }
	    else if (bitLoc == 6)
	    {
            inodeMap[blockLoc] &= 0xfd;
	    }
	    else if (bitLoc == 7)
	    {
            inodeMap[blockLoc] &= 0xfe;
	    }

        int inodeBlock = (blockLoc*8) + bitLoc + INODE_START;

		char insert_buffer[4095];
        memcpy(insert_buffer, inodeMap, 64);
        memcpy((insert_buffer + 64), dataMap, 4031);

        for(i = 0; i < 8; i++)
        {
            block_write(i, insert_buffer + (BLOCK_SIZE * i));
        }
		
		free(superBuff);
		return inodeBlock;
}

void flipBit(int blockNum)
{
		log_msg("Flipping bit...");
		char *superBuff = read_super();
		char superBuffCopy[8*BLOCK_SIZE];
		strcpy(superBuffCopy,superBuff);
		char inodeMap[64];
		char dataMap[4031];
	    memcpy(inodeMap, superBuff, 64);
	    memcpy(dataMap, (superBuff + 64), 4031);
		char superCopyInode[64];
	    char superCopyData[4031];
	    memcpy(superCopyInode,inodeMap,64);
		memcpy(superCopyData,dataMap,4031);

		int bitLoc = -1;
	    int blockLoc = -1;
		int j,i,myBit,myIndex;
		char freeBit,isInode;
		if (blockNum < DATA_START)
		{
			log_msg("Flipping inode");
			myBit = blockNum - INODE_START;

			//modified ceil function
			if(blockNum % 8 > 0)
			{
				myIndex = (blockNum/8)+1;
			}
			else
			{
				myIndex = blockNum/8;
			}

			myIndex -= 1;

			for(j = 7; j >= 0; j--)
			{
				freeBit = superCopyInode[myIndex] & 1;
				if(!freeBit && ((myIndex*8)+j)==myBit)
				{
					bitLoc = j;
					blockLoc = myIndex;
					break;
				}
				superCopyInode[myIndex] >>= 1;
			}
			if (bitLoc == 0)
			{
				inodeMap[blockLoc] ^= 0x80;
			}
			else if (bitLoc == 1)
			{
				inodeMap[blockLoc] ^= 0x40;
			}
			else if (bitLoc == 2)
			{
				inodeMap[blockLoc] ^= 0x20;
			}
			else if (bitLoc == 3)
			{
			    inodeMap[blockLoc] ^= 0x10;
			}
			else if (bitLoc == 4)
			{
		        inodeMap[blockLoc] ^= 0x08;
			}
			else if (bitLoc == 5)
			{
		        inodeMap[blockLoc] ^= 0x04;
			}
			else if (bitLoc == 6)
			{
		        inodeMap[blockLoc] ^= 0x02;
			}
			else if (bitLoc == 7)
			{
		        inodeMap[blockLoc] ^= 0x01;
			}
		}
		else
		{
			myBit = blockNum - DATA_START;
			log_msg("Flipping data");

			if(blockNum % 8 > 0)
			{
				myIndex = (blockNum/8)+1;
			}
			else
			{
				myIndex = blockNum/8;
			}

			myIndex -= 1;

			for(j = 7; j >= 0; j--)
			{
				freeBit = superCopyData[myIndex] & 1;
				if(!freeBit && ((myIndex*8)+j)==myBit)
				{
					bitLoc = j;
					blockLoc = myIndex;
					break;
				}
				superCopyData[myIndex] >>= 1;
			}
			if (bitLoc == 0)
			{
				dataMap[blockLoc] ^= 0x80;
			}
			else if (bitLoc == 1)
			{
				dataMap[blockLoc] ^= 0x40;
			}
			else if (bitLoc == 2)
			{
				dataMap[blockLoc] ^= 0x20;
			}
			else if (bitLoc == 3)
			{
			    dataMap[blockLoc] ^= 0x10;
			}
			else if (bitLoc == 4)
			{
		        dataMap[blockLoc] ^= 0x08;
			}
			else if (bitLoc == 5)
			{
		        dataMap[blockLoc] ^= 0x04;
			}
			else if (bitLoc == 6)
			{
		        dataMap[blockLoc] ^= 0x02;
			}
			else if (bitLoc == 7)
			{
		        dataMap[blockLoc] ^= 0x01;
			}
		}




		char insert_buffer[4095];
        memcpy(insert_buffer, inodeMap, 64);
        memcpy((insert_buffer + 64), dataMap, 4031);

        for(i = 0; i < 8; i++)
        {
            block_write(i, insert_buffer + (BLOCK_SIZE * i));
        }
		
	log_msg("DONE flipping, returning\n");
	free(superBuff);
	return;
}

void removeSubDir(char *fullPath,inode start)
{
	inode dirNode = get_inode(fullPath,start,0);
	char *myFiles = get_buffer(dirNode);
	char *myStartToMyFiles = myFiles;

	char *token = strtok(myFiles, "\n");
	int nodeNumber,dataNumber,myBlockUseNum,retStat,index;
	char *fileName;
	mode_t myMode;
	inode currInode;
	char *fullPathCopy = (char*)malloc(strlen(fullPath)+1);
	char *copy = fullPathCopy;

	while (token != NULL)
	{
			strcpy(fullPathCopy,fullPath);
			fileName = strstr(token, "\t");
			fileName[0] = '\0';
			fileName++;
			fullPathCopy = realloc(fullPathCopy,strlen(fileName)+strlen(fullPathCopy)+2);
			copy = fullPathCopy;
			strcat(fullPathCopy,"/");
			strcat(fullPathCopy,fileName);
			currInode = get_inode(fullPathCopy,start,0);
			myMode = currInode.info.st_mode;
			myMode &= S_IFDIR;
			nodeNumber = atoi(token);

			if (myMode == S_IFDIR)
			{
				if(currInode.info.st_ino != dirNode.info.st_ino)
				{
					removeSubDir(fullPathCopy,start);
					flipBit(nodeNumber);
				
					if(currInode.info.st_size % BLOCK_SIZE > 0)
					{
						myBlockUseNum = (currInode.info.st_size/BLOCK_SIZE)+1;
					}
					else
					{
						myBlockUseNum = currInode.info.st_size/BLOCK_SIZE;
					}

					for(index=0;index<myBlockUseNum;index++)
					{
						flipBit(currInode.direct[index]);
					}

					writeToDirectory(fullPathCopy, MY_DELETE);
					//log_msg("[removeSubDir] nested directory removed\n");
				}
			}
			else
			{
				retStat = sfs_unlink(fullPathCopy);
				//log_msg("[removeSubDir] file just ulinked. retStat:%d\n",retStat);
			}

			//log_msg("[removeSubDir] Is Lance right...\n");
			free(copy);
			//log_msg("[removeSubDir] ...Lance was right\n");
			token = strtok(NULL, "\n");
	}
	
	////log_msg("[removeSubDir] freeing before exit\n");
	//free(copy);
	//log_msg("[removeSubDir] freeing again before exit\n");
	free(myStartToMyFiles);
	//log_msg("[removeSubDir] exit\n");
}

