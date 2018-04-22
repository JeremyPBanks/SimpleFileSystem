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
inode currentNode;
inode rootNode;
int lastOPFlag;//0=Read,1=Write
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
	char rootData[PATH_MAX];
	strcpy(rootData, "8\t.\n");
	bstat = block_write(DATA_START, rootData);
	log_msg("Bstat after write: %d\n", bstat);

	bzero(rootData, PATH_MAX);
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
    char fpath[PATH_MAX];
    strcpy(fpath, path);
    log_msg("fpath: %s, path: %s\n",fpath,path);
    int myLen = strlen(fpath);

    if(strcmp(fpath,"/") == 0 && myLen == 1)
    {
	log_msg("JUST CHANGED / TO /.\n");
	strcpy(fpath, "/.");
    }

    inode start = rootNode;
    /*if(fpath[0] == '/')
    {start = rootNode;}
    else
    {start = currentNode;}*/

    inode myInode = get_inode(fpath, start);
    if(!fileFound)
    {
        return -2; //file not found
    }
    
    log_msg("myInode's number %d and its first block is index %d.\n", myInode.info.st_ino, myInode.direct[0]);

    char *grabbedInode = get_buffer(myInode);

    log_msg("Bruh, I just grabbed this inode: %s\n",grabbedInode);

    *statbuf = myInode.info;
    
    log_msg("\nsfs_getattr(path=\"%s\", statbuf=0x%08x)\n",
	  path, statbuf);
    
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

    char pathCopy[PATH_MAX];
    strcpy(pathCopy,path);
    //TODO: alter file permissions?

    //check if file exists
    inode fileNode = get_inode(pathCopy, rootNode);

    if(!fileFound)//file doesn't exist
    {
	    //make fileNode into new inode
	    char *superBuff = read_super();
	    memcpy(inodeMap, superBuff, 64);
	    memcpy(dataMap, (superBuff + 64), 4031);
	    char superCopyInode[64];
	    char superCopyData[64];
	    memcpy(superCopyInode,inodeMap,64);
	    memcpy(superCopyData,dataMap,4031);
	    int bitLoc = -1;
	    int blockLoc = -1;

	    for(i = 0; i < 64; i++)
	    {
	        for(j = 7; j >= 0; j--)
	        {
		        freeBit = superCopyInode[i] & 1;
	            if(!freeBit)
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


        inode root_inode;
        root_inode.info.st_dev = 0;
        root_inode.info.st_ino = inodeBlock;
        root_inode.info.st_mode = S_IFREG | mode; //regular file with passed-in permissions
        root_inode.info.st_nlink = 1;
        root_inode.info.st_uid = getuid();
        root_inode.info.st_gid = getgid();
        root_inode.info.st_rdev = 0;
        root_inode.info.st_size = 0; //see string in init()

        

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
	            if(!freeBit)
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

        int dataBlock = (bitLoc * 8) + (blockLoc + 520);
        
        root_inode.direct[0] = dataBlock;
        write_to_file(root_inode);

        char insert_buffer[4095];
        memcpy(insert_buffer, inodeMap, 64);
        memcpy((insert_buffer + 64), dataMap, 4031);

        for(i = 0; i < 8; i++)
        {
            block_write(i, insert_buffer + (BLOCK_SIZE * i));
        }

        //TODO: update parent folder after insertion

	    free(superBuff);
    }

    else
    {
	//clear, rewrite file (with same permissions?)
	/*
	    STEPS:
	    -clear blocks in data
	    -reset bitmap where appropriate
	    -change flags?
	    
	*/
        //clear(fileNode);        
        

    }
    
    return retstat;
}

/** Remove a file */
int sfs_unlink(const char *path)
{
    int retstat = 0;
    log_msg("sfs_unlink(path=\"%s\")\n", path);

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
int sfs_open(const char *path, struct fuse_file_info *fi)//0=fail,1=success
{
    int retstat = 0;

    inode start = rootNode;

    char pathCopy[PATH_MAX];

    strcpy(pathCopy,path);

    inode checkInode = get_inode(pathCopy,start);

    if (!fileFound)
    {
	    return 0;
    }

    log_msg("\nsfs_open(path\"%s\", fi=0x%08x)\n",path, fi);

    if (lastOPFlag == 0)//Read
    {
	if ((checkInode.info.st_mode & S_IRUSR) == S_IRUSR || (checkInode.info.st_mode & S_IRWXU) == S_IRWXU)
	{
        log_msg("Open Success.\n");
		return 1;
	}
    }
    else if (lastOPFlag == 1)//Write
    {
	if ((checkInode.info.st_mode & S_IWUSR) == S_IWUSR || (checkInode.info.st_mode & S_IRWXU) == S_IRWXU)
	{
        log_msg("Open Success.\n");
		return 1;
	}
    }
    
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
    lastOPFlag = 0;
    int retstat = 0;
    log_msg("\nsfs_read(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",path, buf, size, offset, fi);

   
    return retstat;
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
    lastOPFlag = 1;
    int retstat = 0;
    log_msg("\nsfs_write(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
	    path, buf, size, offset, fi);
    
    
    return retstat;
}


/** Create a directory */
int sfs_mkdir(const char *path, mode_t mode)
{
    int retstat = 0;
    log_msg("\nsfs_mkdir(path=\"%s\", mode=0%3o)\n",
	    path, mode);
   
    
    return retstat;
}


/** Remove a directory */
int sfs_rmdir(const char *path)
{
    int retstat = 0;
    log_msg("sfs_rmdir(path=\"%s\")\n",
	    path);
    
    
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
    int retstat = 0;
    
    
    return retstat;
}

/** Release directory
 *
 * Introduced in version 2.3
 */
int sfs_releasedir(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;

    
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
    //fill in super struct
    super superBlock;

    superBlock.data_bitmap[0] = 0x7f;
    superBlock.inode_bitmap[0] = 0x7f;
    int i;
    for (i=1;i<4031;i++)
    {
    	superBlock.data_bitmap[i] = 0xff;
    }
    for (i=1;i<64;i++)
    {
    	superBlock.inode_bitmap[i] = 0xff;
    }

    //write super block to flat file
    //TODO: write all 8 blocks
    block_write(0, &superBlock);


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

inode get_inode(char *path, inode this_inode)
{
    //TODO: L: I'll make this recursive for now, we'll see if we can change that later
    inode tgt;

    log_msg("get_inode\n");

    fileFound = 1;

    log_msg("Path in get_inode: %s\n", path);

    if(strstr(path, ROOT_PATH) != NULL)//if root path exists in parameter
    {
	log_msg("Chris: It'll never reach here!\n");
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
	log_msg("Searching for file '%s'.\n", path);
	//TODO: read directory here, get inode

	char *bufferTgt = get_buffer(this_inode);

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
		        log_msg("fnameTgt: %s",fnameTgt);
	            stroffsetTgt = fnameTgt - tokenTgt - 1;
	            tokenTgt[stroffsetTgt] = '\0';
	            strcpy(inumTgt, tokenTgt);
	            tgtNodeTgt = atoi(inumTgt);
	            tgt = read_from_file(tgtNodeTgt);
	            break;
	        }
	        tokenTgt = strtok(NULL, "\n");
        }

        if (tokenTgt == NULL)
        {
            log_msg("Failed to find inode with path: %s\n",path);
	        fileFound = 0;
        }

        free(bufferTgt);
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
    char filename[PATH_MAX];
    int i = newpath - path - 1;
    memcpy(filename, path, i);
    filename[i] = '\0';
    
    log_msg("Searching for folder '%s' [name size %d] in path '%s'.\n", filename, i, path);

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
    log_msg("Newpath is %s\n and tgt is %i", newpath, tgt.info.st_ino);
    return get_inode(newpath, tgt);
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

    char buf[PATH_MAX];
    int bstat = block_read(node, buf);
    if(bstat < 0)
    {
	log_msg("Failed to read from node %d.\n", node);
    }

    char *token = strtok(buf, "\t");

    log_msg("Atoi...\n");
    testnode.info.st_dev = atoi(token);
    log_msg("strtok...\n");
    token = strtok(NULL, "\t");
    log_msg("CRASH...\n");
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
    
    log_msg("Just tokenized!\n");
    

    return testnode;
}

//NOTE: this function returns an allocated string which must be freed
char* get_buffer(inode node)
{
    if(!fileFound)
    {
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

    //TODO: for now, assume directories won't require indirect inodes; total string size less than 32 * 512. (Maybe) fix this later
    for(i = 0; i < len; i++)
    {
	bstat = block_read(node.direct[i], readbuff);
	if(bstat < 0)
	{
	    log_msg("Failed to read block in get_buffer.\n");
	}
	buffer = realloc(buffer, BLOCK_SIZE * (i+1));
	memcpy((buffer + (BLOCK_SIZE * i)), readbuff, BLOCK_SIZE);
	bzero(readbuff, BLOCK_SIZE);
    }

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
