/**
 * @file storage.h
 * @brief Disk storage abstraction for the NUFS filesystem.
 *
 * Assumptions:
 * All paths are absolute (starting with '/')
 * The filesystem is initialized before any operations are called
 * Maximum file size is limited by available blocks
 */
#ifndef NUFS_STORAGE_H
#define NUFS_STORAGE_H

#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "helpers/slist.h"

/**
 * Initialize the storage system.
 *
 * Sets up the disk image, inode table, and root directory.
 * Creates the disk image file if it doesn't exist.
 *
 * @param path Path to the disk image file.
 */
void storage_init(const char *path);

/**
 * Get file/directory attributes.
 *
 * Fills the stat structure with metadata for the file or directory
 * at the given path.
 *
 * @param path Absolute path to the file or directory.
 * @param st Pointer to stat structure to fill.
 *
 * @return 0 on success, -ENOENT if path doesn't exist.
 */
int storage_stat(const char *path, struct stat *st);

/**
 * Read data from a file.
 *
 * Reads up to 'size' bytes starting at 'offset' from the file
 * at the given path.
 *
 * @param path Absolute path to the file.
 * @param buf Buffer to read data into.
 * @param size Maximum number of bytes to read.
 * @param offset Byte offset in the file to start reading from.
 *
 * @return Number of bytes read, or negative error code.
 */
int storage_read(const char *path, char *buf, size_t size, off_t offset);

/**
 * Write data to a file.
 *
 * Writes 'size' bytes from 'buf' to the file at 'offset'.
 * Grows the file if necessary.
 *
 * @param path Absolute path to the file.
 * @param buf Buffer containing data to write.
 * @param size Number of bytes to write.
 * @param offset Byte offset in the file to start writing at.
 *
 * @return Number of bytes written, or negative error code.
 */
int storage_write(const char *path, const char *buf, size_t size, off_t offset);

/**
 * Truncate a file to the given size.
 *
 * If size is smaller than current size, data is lost.
 * If size is larger, the file is extended with zeros.
 *
 * @param path Absolute path to the file.
 * @param size New size in bytes.
 *
 * @return 0 on success, negative error on fail.
 */
int storage_truncate(const char *path, off_t size);

/**
 * Create a new file or directory.
 *
 * Creates a new filesystem object with the given mode.
 * Use S_IFREG for regular files, S_IFDIR for directories.
 *
 * @param path Absolute path for the new file/directory.
 * @param mode File type and permissions.
 *
 * @return 0 on success, negative error on fail.
 */
int storage_mknod(const char *path, int mode);

/**
 * Delete a file.
 *
 * Removes the file at the given path. For directories, use
 * storage_rmdir instead.
 *
 * @param path Absolute path to the file to delete.
 *
 * @return 0 on success, negative error on fail.
 */
int storage_unlink(const char *path);

/**
 * Create a hard link.
 *
 * Creates a new directory entry 'to' pointing to the same inode as 'from'.
 *
 * @param from Absolute path to existing file.
 * @param to Absolute path for the new link.
 *
 * @return 0 on success, negative error on fail.
 */
int storage_link(const char *from, const char *to);

/**
 * Rename or move a file/directory.
 *
 * Moves the file or directory from one path to another.
 * Works for moves within and between directories.
 *
 * @param from Current absolute path.
 * @param to New absolute path.
 *
 * @return 0 on success, negative error on fail.
 */
int storage_rename(const char *from, const char *to);

/**
 * Update file access and modification times.
 *
 * @param path Absolute path to the file.
 * @param ts Array of two timespec: [0] = atime, [1] = mtime.
 *
 * @return 0 on success, negative error on fail.
 */
int storage_set_time(const char *path, const struct timespec ts[2]);

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
slist_t *storage_list(const char *path);

/**
 * Change file mode/permissions.
 *
 * @param path Absolute path to the file.
 * @param mode New mode (permissions and type).
 *
 * @return 0 on success,negative error on fail.
 */
int storage_chmod(const char *path, mode_t mode);

#endif

