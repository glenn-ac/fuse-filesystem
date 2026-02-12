/**
 * @file directory.c
 * @brief Implementation of directory manipulation functions.
 *
 * Manages directories as special files containing directory entries.
 * Each directory entry is 64 bytes, mapping a filename to an inode number.
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "directory.h"
#include "helpers/blocks.h"
#include "helpers/bitmap.h"

/**
 * Initialize the root directory.
 *
 * Allocates inode 0 for the root directory, sets up its mode as a
 * directory, and allocates an initial data block.
 */
void directory_init() {
    // Check if root directory already exists 
    void *ibm = get_inode_bitmap();
    if (bitmap_get(ibm, 0)) {
        printf("+ directory_init: root already exists\n");
        return;
    }
    
    // Allocate inode 0 for root
    int root_inum = alloc_inode();
    if (root_inum != 0) {
        printf("! directory_init: expected inode 0, got %d\n", root_inum);
    }
    
    // Set up root directory inode
    inode_t *root = get_inode(0);
    root->mode = 040755;  // rwxr-xr-x
    root->size = 0;
    
    // Allocate initial block for directory entries
    grow_inode(root, BLOCK_SIZE);
    
    printf("+ directory_init: created root directory\n");
}

/**
 * Get a directory entry at a specific index.
 *
 * Calculates which block the etry is in and returns a pointer to it.
 *
 * @param di Pointer to the directory's inode.
 * @param index Entry index.
 *
 * @return Pointer to the directory entry, or NULL if out of range.
 */
dirent_t *directory_get_entry(inode_t *di, int index) {
    if (di == NULL || index < 0) {
        return NULL;
    }
    
    // Calculate which block and offset within block
    int entries_per_block = BLOCK_SIZE / sizeof(dirent_t);
    int block_index = index / entries_per_block;
    int entry_offset = index % entries_per_block;
    
    // Get the block number
    int bnum = inode_get_bnum(di, block_index);
    if (bnum <= 0) {
        return NULL;
    }
    
    // Get the directory entry
    dirent_t *entries = (dirent_t *)blocks_get_block(bnum);
    return &entries[entry_offset];
}

/**
 * Calculate the maximum number of entries a directory can hold.
 *
 * @param di Pointer to the directory's inode.
 *
 * @return Maximum number of directory entries.
 */
static int directory_max_entries(inode_t *di) {
    int num_blocks = bytes_to_blocks(di->size);
    if (di->size == 0) {
        num_blocks = 0;
    }
    return num_blocks * (BLOCK_SIZE / sizeof(dirent_t));
}

/**
 * Look up a name in a directory.
 *
 * Iterates through all directory entries looking for a matching name.
 *
 * @param di Pointer to the directory's inode.
 * @param name Name to search for.
 *
 * @return Inode number if found, -1 if not found.
 */
int directory_lookup(inode_t *di, const char *name) {
    if (di == NULL || name == NULL || strlen(name) == 0) {
        return -1;
    }
    
    int max_entries = directory_max_entries(di);
    
    for (int ii = 0; ii < max_entries; ++ii) {
        dirent_t *entry = directory_get_entry(di, ii);
        if (entry == NULL) {
            break;
        }
        
        if (entry->inum != 0 && strcmp(entry->name, name) == 0) {
            return entry->inum;
        }
    }
    
    return -1;  
}

/**
 * Add an entry to a directory.
 *
 * Finds an empty slot (inum == 0 or empty name) and adds the new entry.
 * Grows the directory if no empty slots are available.
 *
 * @param di Pointer to the directory's inode.
 * @param name Name for the new entry.
 * @param inum Inode number to associate with the name.
 *
 * @return 0 on success, -1 on failure.
 */
int directory_put(inode_t *di, const char *name, int inum) {
    if (di == NULL || name == NULL || strlen(name) == 0) {
        return -1;
    }
    
    if (strlen(name) >= DIR_NAME_LENGTH) {
        printf("! directory_put: name too long: %s\n", name);
        return -1;
    }
    
    int max_entries = directory_max_entries(di);
    
    // Look for an empty slot
    for (int ii = 0; ii < max_entries; ++ii) {
        dirent_t *entry = directory_get_entry(di, ii);
        if (entry == NULL) {
            break;
        }
        
        // Check if this slot is empty
        if (entry->inum == 0 || entry->name[0] == '\0') {
            strncpy(entry->name, name, DIR_NAME_LENGTH - 1);
            entry->name[DIR_NAME_LENGTH - 1] = '\0';
            entry->inum = inum;
            printf("+ directory_put: added %s -> %d at slot %d\n", name, inum, ii);
            return 0;
        }
    }
    
    // No empty slot found, need to grow the directory
    int old_size = di->size;
    int new_size = old_size + BLOCK_SIZE;
    
    if (grow_inode(di, new_size) < 0) {
        printf("! directory_put: failed to grow directory\n");
        return -1;
    }
    
    // Add entry in the newly allocated space
    int new_entry_index = max_entries;
    dirent_t *entry = directory_get_entry(di, new_entry_index);
    if (entry == NULL) {
        printf("! directory_put: failed to get new entry\n");
        return -1;
    }
    
    strncpy(entry->name, name, DIR_NAME_LENGTH - 1);
    entry->name[DIR_NAME_LENGTH - 1] = '\0';
    entry->inum = inum;
    printf("+ directory_put: added %s -> %d at slot %d (grew dir)\n", name, inum, new_entry_index);
    
    return 0;
}

/**
 * Delete an entry from a directory.
 *
 * Finds the entry with the given name and clears it.
 *
 * @param di Pointer to the directory's inode.
 * @param name Name of the entry to delete.
 *
 * @return 0 on success, -1 if entry not found.
 */
int directory_delete(inode_t *di, const char *name) {
    if (di == NULL || name == NULL || strlen(name) == 0) {
        return -1;
    }
    
    int max_entries = directory_max_entries(di);
    
    for (int ii = 0; ii < max_entries; ++ii) {
        dirent_t *entry = directory_get_entry(di, ii);
        if (entry == NULL) {
            break;
        }
        
        // Check if this entry matches
        if (entry->inum != 0 && strcmp(entry->name, name) == 0) {
            printf("+ directory_delete: removed %s (was -> %d)\n", name, entry->inum);
            memset(entry, 0, sizeof(dirent_t));
            return 0;
        }
    }
    
    return -1;  // Not found
}

/**
 * Print directory contents.
 *
 * @param dd Pointer to the directory's inode.
 */
void print_directory(inode_t *dd) {
    if (dd == NULL) {
        printf("directory: NULL\n");
        return;
    }
    
    printf("directory contents:\n");
    int max_entries = directory_max_entries(dd);
    
    for (int ii = 0; ii < max_entries; ++ii) {
        dirent_t *entry = directory_get_entry(dd, ii);
        if (entry == NULL) {
            break;
        }
        
        if (entry->inum != 0 && entry->name[0] != '\0') {
            printf("  [%d] %s -> %d\n", ii, entry->name, entry->inum);
        }
    }
}

/**
 * Extract the filename from a path.
 *
 * Returns a pointer to the last component of the path.
 *
 * @param path Absolute path.
 *
 * @return Pointer to the filename portion of the path.
 */
const char *get_basename(const char *path) {
    if (path == NULL) {
        return NULL;
    }
    
    const char *last_slash = strrchr(path, '/');
    if (last_slash == NULL) {
        return path;
    }
    
    return last_slash + 1;
}

/**
 * Look up a path and return the inode number.
 *
 * Parses the path and traverses the directory tree from root.
 *
 * @param path Absolute path starting with '/'.
 *
 * @return Inode number if found, -1 if not found.
 */
int tree_lookup(const char *path) {
    if (path == NULL || path[0] != '/') {
        return -1;
    }
    
    // Root directory
    if (strcmp(path, "/") == 0) {
        return 0;
    }
    
    // Split path into components
    slist_t *components = slist_explode(path + 1, '/');  // Skip leading '/'
    
    int current_inum = 0;  // Start at root
    
    for (slist_t *curr = components; curr != NULL; curr = curr->next) {
        // Skip empty components 
        if (strlen(curr->data) == 0) {
            continue;
        }
        
        inode_t *current_dir = get_inode(current_inum);
        if (current_dir == NULL) {
            slist_free(components);
            return -1;
        }
        
        // Make sure current node is a directory
        if ((current_dir->mode & 0040000) == 0) {
            slist_free(components);
            return -1;  // Not a directory
        }
        
        // Look up the next component
        int next_inum = directory_lookup(current_dir, curr->data);
        if (next_inum < 0) {
            slist_free(components);
            return -1;  // Not found
        }
        
        current_inum = next_inum;
    }
    
    slist_free(components);
    return current_inum;
}

/**
 * Get the parent directory's inode number for a path.
 *
 * Traverses to the parent directory of the given path.
 *
 * @param path Absolute path to a file or directory.
 *
 * @return Inode number of the parent directory, or -1 if not found.
 */
int tree_lookup_parent(const char *path) {
    if (path == NULL || path[0] != '/') {
        return -1;
    }
    
    // Root's parent is itself, otherwise invalid
    if (strcmp(path, "/") == 0) {
        return 0;
    }
    
    // Find the last slash
    const char *last_slash = strrchr(path, '/');
    if (last_slash == path) {
        // Path is like "/foo", parent is root
        return 0;
    }
    
    // Create parent path
    int parent_len = last_slash - path;
    char parent_path[parent_len + 1];
    strncpy(parent_path, path, parent_len);
    parent_path[parent_len] = '\0';
    
    return tree_lookup(parent_path);
}

/**
 * List all entries in a directory.
 *
 * Returns a linked list of all filenames in the directory.
 *
 * @param path Path to the directory.
 *
 * @return Linked list of filenames, or NULL if directory not found.
 *         Caller must free with slist_free().
 */
slist_t *directory_list(const char *path) {
    int dir_inum = tree_lookup(path);
    if (dir_inum < 0) {
        return NULL;
    }
    
    inode_t *dir = get_inode(dir_inum);
    if (dir == NULL) {
        return NULL;
    }
    
    // Make sure it's a directory
    if ((dir->mode & 0040000) == 0) {
        return NULL;
    }
    
    slist_t *result = NULL;
    int max_entries = directory_max_entries(dir);
    
    for (int ii = 0; ii < max_entries; ++ii) {
        dirent_t *entry = directory_get_entry(dir, ii);
        if (entry == NULL) {
            break;
        }
        
        if (entry->inum != 0 && entry->name[0] != '\0') {
            result = slist_cons(entry->name, result);
        }
    }
    
    return result;
}

