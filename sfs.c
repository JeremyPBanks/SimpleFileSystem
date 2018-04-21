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

#ifdef HAVE_SYS_XATTR_H
#include <sys/xattr.h>
#endif

#include "log.h"
#include "sfs.h"


/*--------GLOBALS----------*/
inode currentNode;
inode rootNode;

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
	strcpy(rootData, "0\t.\n");
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

    inode start;
    if(fpath[0] == '/')
    {start = rootNode;}
    else
    {start = currentNode;}
    inode i = get_inode(fpath, start);

    *statbuf = i.info;
    
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
    int retstat = 0;
    log_msg("\nsfs_create(path=\"%s\", mode=0%03o, fi=0x%08x)\n",
	    path, mode, fi);
    
    
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
int sfs_open(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;
    log_msg("\nsfs_open(path\"%s\", fi=0x%08x)\n",
	    path, fi);

    
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
    int retstat = 0;
    log_msg("\nsfs_read(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
	    path, buf, size, offset, fi);

   
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
    for (i=1;i<4073;i++)
    {
    	superBlock.data_bitmap[i] = 0xff;
    }
    for (i=1;i<64;i++)
    {
    	superBlock.inode_bitmap[i] = 0xff;
    }

    //write super block to flat file
    block_write(0, &superBlock);
/*
    char buff[PATH_MAX];
    block_read(0, buff);
    log_msg("Super data: %s\n", buff);
*/

    //fill in root inode
    inode root_inode;
    root_inode.info.st_dev = 0;
    root_inode.info.st_ino = INODE_START;
    root_inode.info.st_mode = (S_IFDIR & S_IFMT) | (S_IRWXU) | (S_IRWXO); //TODO: fix this up at some point
    root_inode.info.st_nlink = 1;
    root_inode.info.st_uid = 0;
    root_inode.info.st_gid = 0;
    root_inode.info.st_rdev = 0;
    root_inode.info.st_size = 5; //see string in init()
    root_inode.direct[0] = DATA_START;

    for(i = 1; i < 10; i++)
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
}

inode get_inode(char *path, inode root_inode)
{
    //TODO: L: I'll make this recursive for now, we'll see if we can change that later
    inode tgt;

    //skip starting slash to avoid directory confusion (i.e. starting vs. not starting from root
    if(path[0] == '/')
    {path++;}

    //base case
    if(strstr(path, "/") == NULL)
    {
	log_msg("Searching for file '%s'.\n", path);
	//TODO: read directory here, get inode

	//tgt = ???
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

    char buffer[PATH_MAX], readbuff[BLOCK_SIZE];
    buffer[0] = '\0';
    int len = ceil((double)root_inode.info.st_size/BLOCK_SIZE);

    //TODO: for now, assume directories won't require indirect inodes. Fix this later
    for(i = 0; i < len; i++)
    {
	block_read(root_inode.direct[i], readbuff);
	strcat(buffer, readbuff);
	bzero(readbuff, BLOCK_SIZE);
    }

    //TODO: parse string to get inode info here

    return get_inode(newpath, newRoot);
}

void write_to_file(inode insert_inode)
{
    char *rootString;
    asprintf(&rootString, "%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t", insert_inode.info.st_dev, insert_inode.info.st_ino, insert_inode.info.st_mode, insert_inode.info.st_nlink, insert_inode.info.st_uid, insert_inode.info.st_gid, insert_inode.info.st_rdev, insert_inode.info.st_size, insert_inode.direct[0], insert_inode.direct[1], insert_inode.direct[2], insert_inode.direct[3], insert_inode.direct[4], insert_inode.direct[5], insert_inode.direct[6], insert_inode.direct[7], insert_inode.direct[8], insert_inode.direct[9], insert_inode.direct[10], insert_inode.direct[11], insert_inode.indirect[0], insert_inode.indirect[1], insert_inode.info.st_atime, insert_inode.info.st_mtime, insert_inode.info.st_ctime, insert_inode.info.st_blksize, insert_inode.info.st_blocks);

    int bstat = block_write(INODE_START, rootString);
    log_msg("After block write\n");
    free(rootString);
}


inode read_from_file(int block)
{
    inode testnode;

    char buf[PATH_MAX];
    int bstat = block_read(block, buf);
    log_msg("After block read\n");
    char *token = strtok(buf, "\t");
    log_msg("About to tokenize...\n");

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
    log_msg("First Direct\n");
    testnode.direct[0] = atoi(token);
    token = strtok(NULL, "\t");
    log_msg("Second Direct\n");
    testnode.direct[1] = atoi(token);
    token = strtok(NULL, "\t");
    testnode.direct[2] = atoi(token);
    token = strtok(NULL, "\t");
    testnode.direct[3] = atoi(token);
    token = strtok(NULL, "\t");
    testnode.direct[4] = atoi(token);
    token = strtok(NULL, "\t");
    testnode.direct[5] = atoi(token);
    token = strtok(NULL, "\t");
    testnode.direct[6] = atoi(token);
    token = strtok(NULL, "\t");
    testnode.direct[7] = atoi(token);
    token = strtok(NULL, "\t");
    testnode.direct[8] = atoi(token);
    token = strtok(NULL, "\t");
    testnode.direct[9] = atoi(token);
    token = strtok(NULL, "\t");
    testnode.direct[10] = atoi(token);
    token = strtok(NULL, "\t");
    testnode.direct[11] = atoi(token);
    token = strtok(NULL, "\t");
    log_msg("Indirect Time!\n");
    testnode.indirect[0] = atoi(token);
    token = strtok(NULL, "\t");
    testnode.indirect[1] = atoi(token);
    token = strtok(NULL, "\t");

    log_msg("Time, Mr. Freeman?\n");
    testnode.info.st_atime = atoi(token);
    token = strtok(NULL, "\t");
    log_msg("Game Scientist\n");
    testnode.info.st_mtime = atoi(token);
    token = strtok(NULL, "\t");
    testnode.info.st_ctime = atoi(token);
    log_msg("Black Mesa\n");
    token = strtok(NULL, "\t");
    testnode.info.st_blksize = atoi(token);
    token = strtok(NULL, "\t");
    log_msg("Dog\n");
    testnode.info.st_blocks = atoi(token);
    
    log_msg("Just tokenized!\n");

    return testnode;
}


