/**
 * @file inode.h
 * @brief Inode manipulation routines for the NUFS filesystem.
 *
 * Provides functions to allocate, free, and manipulate inodes which represent
 * files and directories in the filesystem. Each inode stores metadata and
 * block pointers for a single file or directory.
 *
 * Assumptions:
 * Block 1 is reserved for the inode table
 * Maximum of 128 inodes
 * Inode 0 is always the root directory
 */
#ifndef INODE_H
#define INODE_H

#include <sys/stat.h>
#include <time.h>

#define INODE_COUNT 128

/**
 * Inode structure representing a file or directory.
 *
 */
typedef struct inode {
    int refs;           // reference count 
    int mode;           // permission & type
    int size;           // file size in bytes
    int block;          // direct block pointer 
    int indirect;       // indirect block pointer 
    time_t atime;       // last access time
    time_t mtime;       // last modification time
    int uid;            // owner user ID
    int gid;            // owner group ID
} inode_t;

/**
 * Print inode information for debugging.
 *
 * @param node Pointer to the inode to print.
 */
void print_inode(inode_t *node);

/**
 * Get a pointer to the inode with the given inode number.
 *
 * @param inum Inode number 
 *
 * @return Pointer to the inode structure in the inode table block.
 *         Returns NULL if inum is out of range.
 */
inode_t *get_inode(int inum);

/**
 * Allocate a new inode.
 *
 * Finds the first free inode in the inode bitmap, marks it as used,
 * and initializes the inode structure.
 *
 * @return The inode number of the newly allocated inode, or -1 if
 *         no free inodes are available.
 */
int alloc_inode();

/**
 * Free an inode and release its data blocks.
 *
 * Clears the inode bitmap entry and frees all blocks associated
 * with the inode, direct and indirect.
 *
 * @param inum Inode number to free.
 */
void free_inode(int inum);

/**
 * Grow an inode to accommodate the given size.
 *
 * Allocates additional blocks as needed to store 'size' bytes.
 * Updates the inode's size field.
 *
 * @param node Pointer to the inode to grow.
 * @param size New size in bytes.
 *
 * @return 0 on success, -1 if blocks couldn't be allocated.
 */
int grow_inode(inode_t *node, int size);

/**
 * Shrink an inode to the given size.
 *
 * Frees blocks that are no longer needed and updates the size field.
 *
 * @param node Pointer to the inode to shrink.
 * @param size New size in bytes.
 *
 * @return 0 on success.
 */
int shrink_inode(inode_t *node, int size);

/**
 * Get the block number for a given file block index.
 *
 * Translates a logical block number within a file to
 * the actual disk block number where the data is stored.
 *
 * @param node Pointer to the inode.
 * @param file_bnum Logical block number within the file.
 *
 * @return Disk block number, or -1 if the block is not allocated.
 */
int inode_get_bnum(inode_t *node, int file_bnum);

/**
 * Initialize the inode table.
 *
 * Marks block 1 as used for the inode table. Should be called
 * during filesystem initialization after blocks_init().
 */
void inode_init();

#endif

