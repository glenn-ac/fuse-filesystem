/**
 * @file inode.c
 * @brief Implementation of inode manipulation routines.
 *
 * Manages the inode table stored in block 1 of the filesystem.
 * Provides allocation, deallocation, and block management for inodes.
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "inode.h"
#include "helpers/blocks.h"
#include "helpers/bitmap.h"

// Block number where the inode table is stored
#define INODE_TABLE_BLOCK 1

/**
 * Print inode information for debugging.
 *
 * @param node Pointer to the inode to print.
 */
void print_inode(inode_t *node) {
    if (node == NULL) {
        printf("inode: NULL\n");
        return;
    }
    printf("inode: refs=%d, mode=%o, size=%d, block=%d, indirect=%d\n",
           node->refs, node->mode, node->size, node->block, node->indirect);
}

/**
 * Get a pointer to the inode with the given inode number.
 *
 * The inode table is stored in block 1. Each inode is accessed
 * by calculating its offset within that block.
 *
 * @param inum Inode number 
 *
 * @return Pointer to the inode structure, or NULL if inum is invalid.
 */
inode_t *get_inode(int inum) {
    if (inum < 0 || inum >= INODE_COUNT) {
        return NULL;
    }
    
    inode_t *inode_table = (inode_t *)blocks_get_block(INODE_TABLE_BLOCK);
    return &inode_table[inum];
}

/**
 * Allocate a new inode.
 *
 * @return Inode number of newly allocated inode, or -1 if none available.
 */
int alloc_inode() {
    void *ibm = get_inode_bitmap();
    
    // Find first free inode
    for (int ii = 0; ii < INODE_COUNT; ++ii) {
        if (!bitmap_get(ibm, ii)) {
            // Mark as used
            bitmap_put(ibm, ii, 1);
            
            // Initialize the inode
            inode_t *node = get_inode(ii);
            memset(node, 0, sizeof(inode_t));
            node->refs = 1;
            node->mode = 0;
            node->size = 0;
            node->block = 0;
            node->indirect = 0;
            node->uid = getuid();
            node->gid = getgid();
            node->atime = time(NULL);
            node->mtime = time(NULL);
            
            printf("+ alloc_inode() -> %d\n", ii);
            return ii;
        }
    }
    
    return -1;  // No free inodes
}

/**
 * Free an inode and all its associated data blocks.
 *
 * Releases the direct block and all indirect blocks, then clears
 * the inode bitmap entry.
 *
 * @param inum Inode number to free.
 */
void free_inode(int inum) {
    if (inum < 0 || inum >= INODE_COUNT) {
        return;
    }
    
    printf("+ free_inode(%d)\n", inum);
    
    inode_t *node = get_inode(inum);
    
    // Free the direct block if allocated
    if (node->block != 0) {
        free_block(node->block);
    }
    
    // Free indirect blocks if allocated
    if (node->indirect != 0) {
        int *indirect_table = (int *)blocks_get_block(node->indirect);
        int num_indirect = bytes_to_blocks(node->size) - 1;
        
        // Free each block pointed to by the indirect table
        for (int ii = 0; ii < num_indirect && ii < BLOCK_SIZE / (int)sizeof(int); ++ii) {

            if (indirect_table[ii] != 0) {
                free_block(indirect_table[ii]);
            }
        }
        
        // Free the indirect block itself
        free_block(node->indirect);
    }
    
    // Clear the inode
    memset(node, 0, sizeof(inode_t));
    
    // Mark as free in bitmap
    void *ibm = get_inode_bitmap();
    bitmap_put(ibm, inum, 0);
}

/**
 * Get the block number for a given file block index.
 *
 * Block 0 is stored in the direct pointer. Blocks 1+ are stored
 * in the indirect block table.
 *
 * @param node Pointer to the inode.
 * @param file_bnum Logical block number within the file.
 *
 * @return Disk block number, or -1 if not allocated.
 */
int inode_get_bnum(inode_t *node, int file_bnum) {
    if (node == NULL || file_bnum < 0) {
        return -1;
    }
    
    // First block is the direct pointer
    if (file_bnum == 0) {
        return node->block;
    }
    
    // Subsequent blocks are in the indirect table
    if (node->indirect == 0) {
        return -1; 
    }
    
    int *indirect_table = (int *)blocks_get_block(node->indirect);
    int indirect_index = file_bnum - 1;  // Offset by 1 since block 0 is direct
    
    if (indirect_index >= BLOCK_SIZE / (int)sizeof(int)) {
        return -1;  
    }
    
    return indirect_table[indirect_index];
}

/**
 * Grow an inode to accommodate the given size.
 *
 * @param node Pointer to the inode.
 * @param size New size in bytes.
 *
 * @return 0 on success, -1 on failure.
 */
int grow_inode(inode_t *node, int size) {
    if (node == NULL) {
        return -1;
    }
    
    int current_blocks = bytes_to_blocks(node->size);
    int target_blocks = bytes_to_blocks(size);
    
    // Size 0 needs 0 blocks
    if (size == 0) {
        target_blocks = 0;
    }
    if (node->size == 0) {
        current_blocks = 0;
    }
    
    printf("+ grow_inode: %d blocks -> %d blocks\n", current_blocks, target_blocks);
    
    // Allocate blocks as needed
    for (int ii = current_blocks; ii < target_blocks; ++ii) {
        int new_block = alloc_block();
        if (new_block < 0) {
            return -1;  // No free 
        }
        
        // Zero out the new block
        memset(blocks_get_block(new_block), 0, BLOCK_SIZE);
        
        if (ii == 0) {
            // First block goes in direct pointer
            node->block = new_block;
        } else {
            // Need indirect block for additional blocks
            if (node->indirect == 0) {
                node->indirect = alloc_block();
                if (node->indirect < 0) {
                    free_block(new_block);
                    return -1;
                }
                memset(blocks_get_block(node->indirect), 0, BLOCK_SIZE);
            }
            
            int *indirect_table = (int *)blocks_get_block(node->indirect);
            indirect_table[ii - 1] = new_block;
        }
    }
    
    node->size = size;
    node->mtime = time(NULL);
    
    return 0;
}

/**
 * Shrink an inode to the given size.
 *
 * Frees blocks that are no longer needed.
 *
 * @param node Pointer to the inode.
 * @param size New size in bytes.
 *
 * @return 0 on success.
 */
int shrink_inode(inode_t *node, int size) {
    if (node == NULL) {
        return -1;
    }
    
    int current_blocks = bytes_to_blocks(node->size);
    int target_blocks = bytes_to_blocks(size);
    
    // Handle edge case
    if (size == 0) {
        target_blocks = 0;
    }
    if (node->size == 0) {
        current_blocks = 0;
    }
    
    printf("+ shrink_inode: %d blocks -> %d blocks\n", current_blocks, target_blocks);
    
    // Free blocks from the end
    for (int ii = current_blocks - 1; ii >= target_blocks; --ii) {
        if (ii == 0) {
            // Free direct block
            if (node->block != 0) {
                free_block(node->block);
                node->block = 0;
            }
        } else {
            // Free from indirect table
            if (node->indirect != 0) {
                int *indirect_table = (int *)blocks_get_block(node->indirect);
                if (indirect_table[ii - 1] != 0) {
                    free_block(indirect_table[ii - 1]);
                    indirect_table[ii - 1] = 0;
                }
            }
        }
    }
    
    // If we're down to 0 or 1 blocks, free the indirect block
    if (target_blocks <= 1 && node->indirect != 0) {
        free_block(node->indirect);
        node->indirect = 0;
    }
    
    node->size = size;
    node->mtime = time(NULL);
    
    return 0;
}

/**
 * Initialize the inode table.
 *
 * Marks block 1 as used for the inode table. This should be called
 * during filesystem initialization.
 */
void inode_init() {
    // Mark block 1 as used for inode table
    void *bbm = get_blocks_bitmap();
    bitmap_put(bbm, INODE_TABLE_BLOCK, 1);
    
    printf("+ inode_init: block %d reserved for inode table\n", INODE_TABLE_BLOCK);
}

