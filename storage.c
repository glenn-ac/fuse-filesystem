/**
 * @file storage.c
 * @brief Implementation of the storage abstraction layer.
 *
 * Provides high-level file and directory operations that bridge
 * FUSE callbacks to the underlying inode and directory systems.
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#include "storage.h"
#include "inode.h"
#include "directory.h"
#include "helpers/blocks.h"

/**
 * Initialize the storage system.
 *
 * Sets up the disk image, initializes the inode table, and creates
 * the root directory if it doesn't already exist.
 *
 * @param path Path to the disk image file.
 */
void storage_init(const char *path) {
    printf("+ storage_init(%s)\n", path);
    
    // Initialize the block system
    blocks_init(path);
    
    // Initialize the inode table, reserves block 1
    inode_init();
    
    // Initialize the root directory
    directory_init();
}

/**
 * Get file/directory attributes.
 *
 * @param path Absolute path to the file or directory.
 * @param st Pointer to stat structure to fill.
 *
 * @return 0 on success, -ENOENT if path doesn't exist.
 */
int storage_stat(const char *path, struct stat *st) {
    int inum = tree_lookup(path);
    if (inum < 0) {
        return -ENOENT;
    }
    
    inode_t *node = get_inode(inum);
    if (node == NULL) {
        return -ENOENT;
    }
    
    memset(st, 0, sizeof(struct stat));
    st->st_mode = node->mode;
    st->st_size = node->size;
    st->st_uid = node->uid;
    st->st_gid = node->gid;
    st->st_nlink = node->refs;
    st->st_atime = node->atime;
    st->st_mtime = node->mtime;
    st->st_ino = inum;
    
    // Calculate blocks used for st_blocks, in 512-byte units
    st->st_blocks = (node->size + 511) / 512;
    st->st_blksize = BLOCK_SIZE;
    
    return 0;
}

/**
 * Read data from a file.
 *
 * Reads data from the file's data blocks starting at the given offset.
 *
 * @param path Absolute path to the file.
 * @param buf Buffer to read data into.
 * @param size Maximum number of bytes to read.
 * @param offset Byte offset in the file to start reading from.
 *
 * @return Number of bytes read, or negative error code.
 */
int storage_read(const char *path, char *buf, size_t size, off_t offset) {
    int inum = tree_lookup(path);
    if (inum < 0) {
        return -ENOENT;
    }
    
    inode_t *node = get_inode(inum);
    if (node == NULL) {
        return -ENOENT;
    }
    
    // Check if offset is beyond file size
    if (offset >= node->size) {
        return 0;
    }
    
    // Limit read size to remaining data
    if (offset + (off_t)size > node->size) {
        size = node->size - offset;
    }
    
    size_t bytes_read = 0;
    
    while (bytes_read < size) {
        // Calculate which block and offset within block
        int file_block = (offset + bytes_read) / BLOCK_SIZE;
        int block_offset = (offset + bytes_read) % BLOCK_SIZE;
        
        // Get the actual disk block number
        int bnum = inode_get_bnum(node, file_block);
        if (bnum <= 0) {
            break;  // No more blocks
        }
        
        // Calculate how much to read from this block
        size_t to_read = BLOCK_SIZE - block_offset;
        if (to_read > size - bytes_read) {
            to_read = size - bytes_read;
        }
        
        // Read from the block
        char *block_data = blocks_get_block(bnum);
        memcpy(buf + bytes_read, block_data + block_offset, to_read);
        
        bytes_read += to_read;
    }
    
    // Update access time
    node->atime = time(NULL);
    
    return bytes_read;
}

/**
 * Write data to a file.
 *
 * Grows the file if necessary to accommodate the write.
 *
 * @param path Absolute path to the file.
 * @param buf Buffer containing data to write.
 * @param size Number of bytes to write.
 * @param offset Byte offset in the file to start writing at.
 *
 * @return Number of bytes written, or negative error code.
 */
int storage_write(const char *path, const char *buf, size_t size, off_t offset) {
    int inum = tree_lookup(path);
    if (inum < 0) {
        return -ENOENT;
    }
    
    inode_t *node = get_inode(inum);
    if (node == NULL) {
        return -ENOENT;
    }
    
    // Grow file if necessary
    off_t end_pos = offset + size;
    if (end_pos > node->size) {
        if (grow_inode(node, end_pos) < 0) {
            return -ENOSPC;  // No space
        }
    }
    
    size_t bytes_written = 0;
    
    while (bytes_written < size) {
        // Calculate which block and offset within block
        int file_block = (offset + bytes_written) / BLOCK_SIZE;
        int block_offset = (offset + bytes_written) % BLOCK_SIZE;
        
        // Get the actual disk block number
        int bnum = inode_get_bnum(node, file_block);
        if (bnum <= 0) {
            printf("! storage_write: no block for file_block %d\n", file_block);
            break;
        }
        
        // Calculate how much to write to this block
        size_t to_write = BLOCK_SIZE - block_offset;
        if (to_write > size - bytes_written) {
            to_write = size - bytes_written;
        }
        
        // Write to the block
        char *block_data = blocks_get_block(bnum);
        memcpy(block_data + block_offset, buf + bytes_written, to_write);
        
        bytes_written += to_write;
    }
    
    // Update modification time
    node->mtime = time(NULL);
    
    return bytes_written;
}

/**
 * Truncate a file to the given size.
 *
 * Grows or shrinks the file to the specified size.
 *
 * @param path Absolute path to the file.
 * @param size New size in bytes.
 *
 * @return 0 on success, negative error on fail.
 */
int storage_truncate(const char *path, off_t size) {
    int inum = tree_lookup(path);
    if (inum < 0) {
        return -ENOENT;
    }
    
    inode_t *node = get_inode(inum);
    if (node == NULL) {
        return -ENOENT;
    }
    
    if (size > node->size) {
        return grow_inode(node, size);
    } else if (size < node->size) {
        return shrink_inode(node, size);
    }
    
    return 0;
}

/**
 * Create a new file or directory.
 *
 * Allocates a new inode and adds an entry in the parent directory.
 *
 * @param path Absolute path for the new file/directory.
 * @param mode File type and permissions.
 *
 * @return 0 on success, negative error on fail.
 */
int storage_mknod(const char *path, int mode) {
    // Check if file already exists
    if (tree_lookup(path) >= 0) {
        return -EEXIST;
    }
    
    // Get parent directory
    int parent_inum = tree_lookup_parent(path);
    if (parent_inum < 0) {
        return -ENOENT;  // Parent doesn't exist
    }
    
    inode_t *parent = get_inode(parent_inum);
    if (parent == NULL) {
        return -ENOENT;
    }
    
    // Allocate new inode
    int new_inum = alloc_inode();
    if (new_inum < 0) {
        return -ENOSPC;  // No free inodes
    }
    
    inode_t *new_node = get_inode(new_inum);
    new_node->mode = mode;
    new_node->size = 0;
    
    // If creating a directory, allocate an initial block
    if ((mode & S_IFDIR) != 0) {
        grow_inode(new_node, BLOCK_SIZE);
    }
    
    // Add entry to parent directory
    const char *name = get_basename(path);
    if (directory_put(parent, name, new_inum) < 0) {
        free_inode(new_inum);
        return -ENOSPC;
    }
    
    printf("+ storage_mknod(%s, %o) -> inode %d\n", path, mode, new_inum);
    return 0;
}

/**
 * Delete a file.
 *
 * Decrements the reference count and frees the inode if no more links.
 * Removes the directory entry from the parent.
 *
 * @param path Absolute path to the file to delete.
 *
 * @return 0 on success, negative error on fail.
 */
int storage_unlink(const char *path) {
    int inum = tree_lookup(path);
    if (inum < 0) {
        return -ENOENT;
    }
    
    // Get parent directory
    int parent_inum = tree_lookup_parent(path);
    if (parent_inum < 0) {
        return -ENOENT;
    }
    
    inode_t *parent = get_inode(parent_inum);
    if (parent == NULL) {
        return -ENOENT;
    }
    
    // Remove from parent directory
    const char *name = get_basename(path);
    if (directory_delete(parent, name) < 0) {
        return -ENOENT;
    }
    
    // Decrement reference count
    inode_t *node = get_inode(inum);
    node->refs--;
    
    // Free inode if no more references
    if (node->refs <= 0) {
        free_inode(inum);
    }
    
    printf("+ storage_unlink(%s)\n", path);
    return 0;
}

/**
 * Create a hard link.
 *
 * Creates a new directory entry pointing to an existing inode.
 *
 * @param from Absolute path to existing file.
 * @param to Absolute path for the new link.
 *
 * @return 0 on success, negative error on fail.
 */
int storage_link(const char *from, const char *to) {
    // Get source inode
    int from_inum = tree_lookup(from);
    if (from_inum < 0) {
        return -ENOENT;
    }
    
    // Check if destination already exists
    if (tree_lookup(to) >= 0) {
        return -EEXIST;
    }
    
    // Get parent directory of destination
    int parent_inum = tree_lookup_parent(to);
    if (parent_inum < 0) {
        return -ENOENT;
    }
    
    inode_t *parent = get_inode(parent_inum);
    if (parent == NULL) {
        return -ENOENT;
    }
    
    // Add entry to parent directory
    const char *name = get_basename(to);
    if (directory_put(parent, name, from_inum) < 0) {
        return -ENOSPC;
    }
    
    // Increment reference count
    inode_t *node = get_inode(from_inum);
    node->refs++;
    
    printf("+ storage_link(%s => %s)\n", from, to);
    return 0;
}

/**
 * Rename or move a file/directory.
 *
 * Removes the entry from the old location and adds it to the new location.
 *
 * @param from Current absolute path.
 * @param to New absolute path.
 *
 * @return 0 on success, negative error on fail.
 */
int storage_rename(const char *from, const char *to) {
    // Get source inode
    int from_inum = tree_lookup(from);
    if (from_inum < 0) {
        return -ENOENT;
    }
    
    // If destination exists, remove it first
    int to_inum = tree_lookup(to);
    if (to_inum >= 0) {
        storage_unlink(to);
    }
    
    // Get parent directories
    int from_parent_inum = tree_lookup_parent(from);
    int to_parent_inum = tree_lookup_parent(to);
    
    if (from_parent_inum < 0 || to_parent_inum < 0) {
        return -ENOENT;
    }
    
    inode_t *from_parent = get_inode(from_parent_inum);
    inode_t *to_parent = get_inode(to_parent_inum);
    
    if (from_parent == NULL || to_parent == NULL) {
        return -ENOENT;
    }
    
    // Add to new location first
    const char *to_name = get_basename(to);
    if (directory_put(to_parent, to_name, from_inum) < 0) {
        return -ENOSPC;
    }
    
    // Remove from old location
    const char *from_name = get_basename(from);
    directory_delete(from_parent, from_name);
    
    printf("+ storage_rename(%s => %s)\n", from, to);
    return 0;
}

/**
 * Update file access and modification times.
 *
 * @param path Absolute path to the file.
 * @param ts Array of two timespec: [0] = atime, [1] = mtime.
 *
 * @return 0 on success, negative error on fail.
 */
int storage_set_time(const char *path, const struct timespec ts[2]) {
    int inum = tree_lookup(path);
    if (inum < 0) {
        return -ENOENT;
    }
    
    inode_t *node = get_inode(inum);
    if (node == NULL) {
        return -ENOENT;
    }
    
    node->atime = ts[0].tv_sec;
    node->mtime = ts[1].tv_sec;
    
    return 0;
}

/**
 * List directory contents.
 *
 * Returns a linked list of all entries in the directory.
 *
 * @param path Absolute path to the directory.
 *
 * @return Linked list of filenames, or NULL if not a directory.
 *         Caller must free with slist_free().
 */
slist_t *storage_list(const char *path) {
    return directory_list(path);
}

/**
 * Change file mode/permissions.
 *
 * @param path Absolute path to the file.
 * @param mode New mode (permissions).
 *
 * @return 0 on success, negative error on fail.
 */
int storage_chmod(const char *path, mode_t mode) {
    int inum = tree_lookup(path);
    if (inum < 0) {
        return -ENOENT;
    }
    
    inode_t *node = get_inode(inum);
    if (node == NULL) {
        return -ENOENT;
    }
    
    // Preserve file type, update permissions
    node->mode = (node->mode & S_IFMT) | (mode & ~S_IFMT);
    
    return 0;
}

