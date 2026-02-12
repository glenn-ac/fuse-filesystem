/**
 * @file directory.h
 * @brief Directory manipulation functions for the NUFS filesystem.
 *
 * Provides functions to manage directories, which are special files containing
 * directory entries. Supports nested directories through path traversal.
 *
 * Assumptions:
 * Directory entries are 64 bytes each 
 * Filenames can be up to 47 characters 
 * Inode 0 is always the root directory
 * Empty directory slots have inum = 0 and empty name
 */
#ifndef DIRECTORY_H
#define DIRECTORY_H

#include "inode.h"
#include "helpers/slist.h"

// Maximum length of a filename 
#define DIR_NAME_LENGTH 48

/**
 * Directory entry structure.
 *
 * Each entry maps a filename to an inode number. Empty entries
 * are indicated by inum = 0 or an empty name.
 *
 * Fields:
 * - name: Null-terminated filename (max 47 chars + null)
 * - inum: Inode number of the file/directory
 * - _reserved: Padding to make struct exactly 64 bytes
 */
typedef struct dirent {
    char name[DIR_NAME_LENGTH];  // filename
    int inum;                    // inode number
    char _reserved[12];          // padding to 64 bytes
} dirent_t;

// Number of directory entries per block 
#define DIR_ENTRIES_PER_BLOCK (BLOCK_SIZE / sizeof(dirent_t))

/**
 * Initialize the root directory.
 *
 * Creates inode 0 as a directory and sets up an empty root directory.
 * Should be called during filesystem initialization after inode_init().
 */
void directory_init();

/**
 * Look up a name in a directory.
 *
 * Searches the directory for an entry with the given name.
 *
 * @param di Pointer to the directory's inode.
 * @param name Name to search for.
 *
 * @return Inode number if found, -1 if not found.
 */
int directory_lookup(inode_t *di, const char *name);

/**
 * Add an entry to a directory.
 *
 * Finds an empty slot in the directory and adds the name/inum mapping.
 * Grows the directory if needed to accommodate the new entry.
 *
 * @param di Pointer to the directory's inode.
 * @param name Name for the new entry (max 47 chars).
 * @param inum Inode number to associate with the name.
 *
 * @return 0 on success, -1 on failure.
 */
int directory_put(inode_t *di, const char *name, int inum);

/**
 * Delete an entry from a directory.
 *
 * @param di Pointer to the directory's inode.
 * @param name Name of the entry to delete.
 *
 * @return 0 on success, -1 if entry not found.
 */
int directory_delete(inode_t *di, const char *name);

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
slist_t *directory_list(const char *path);

/**
 * Print directory contents for debugging.
 *
 * @param dd Pointer to the directory's inode.
 */
void print_directory(inode_t *dd);

/**
 * Look up a path and return the inode number.
 *
 * Traverses the directory tree following the path components
 * to find the target file or directory.
 *
 * @param path Absolute path starting with '/'.
 *
 * @return Inode number if found, -1 if not found.
 */
int tree_lookup(const char *path);

/**
 * Get the parent directory's inode number for a path.
 *
 * @param path Absolute path to a file or directory.
 *
 * @return Inode number of the parent directory, or -1 if not found.
 */
int tree_lookup_parent(const char *path);

/**
 * Extract the filename from a path.
 *
 * Returns a pointer to the last component of the path.
 * The returned pointer points into the original path string.
 *
 * @param path Absolute path.
 *
 * @return Pointer to the filename portion of the path.
 */
const char *get_basename(const char *path);

/**
 * Get directory entry at a specific index.
 *
 * @param di Pointer to the directory's inode.
 * @param index Entry index.
 *
 * @return Pointer to the directory entry, or NULL if out of range.
 */
dirent_t *directory_get_entry(inode_t *di, int index);

#endif

