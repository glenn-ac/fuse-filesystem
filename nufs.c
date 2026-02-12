/**
 * @file nufs.c
 * @brief NUFS - A simple FUSE filesystem implementation.
 *
 * Implements the FUSE callbacks for a basic filesystem that supports:
 * File creation, reading, writing, and deletion
 * Nested directories with mkdir, rmdir, and listing
 * File renaming and moving between directories
 * Hard links
 * File truncation and timestamps
 *
 * The filesystem stores data in a 1MB disk image file with 256 4KB blocks.
 */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define FUSE_USE_VERSION 26
#include <fuse.h>

#include "storage.h"
#include "helpers/slist.h"

/**
 * Check if a file exists and is accessible.
 *
 * Implementation for: man 2 access
 *
 * @param path Path to check.
 * @param mask Access mode to check (F_OK, R_OK, W_OK, X_OK).
 *
 * @return 0 if accessible, -ENOENT if not found.
 */
int nufs_access(const char *path, int mask) {
    int rv = 0;
    struct stat st;
    
    rv = storage_stat(path, &st);
    
    printf("access(%s, %04o) -> %d\n", path, mask, rv);
    return rv;
}

/**
 * Get file or directory attributes.
 *
 * Implementation for: man 2 stat
 *
 * @param path Path to the file or directory.
 * @param st Stat structure to fill with attributes.
 *
 * @return 0 on success, -ENOENT if not found.
 */
int nufs_getattr(const char *path, struct stat *st) {
    int rv = storage_stat(path, st);
    
    printf("getattr(%s) -> (%d) {mode: %04o, size: %ld}\n", 
           path, rv, st->st_mode, st->st_size);
    return rv;
}

/**
 * List the contents of a directory.
 *
 * Implementation for: man 2 readdir
 *
 * @param path Path to the directory.
 * @param buf Buffer for directory entries.
 * @param filler Callback function to add entries.
 * @param offset Offset in directory.
 * @param fi File info.
 *
 * @return 0 on success.
 */
int nufs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                 off_t offset, struct fuse_file_info *fi) {
    struct stat st;
    int rv;
    
    // Add . entry, current directory
    rv = storage_stat(path, &st);
    if (rv < 0) {
        printf("readdir(%s) -> %d (dir not found)\n", path, rv);
        return rv;
    }
    filler(buf, ".", &st, 0);
    
    // Add .. entry, parent directiory
    filler(buf, "..", NULL, 0);
    
    // Get list of directory entries
    slist_t *entries = storage_list(path);
    
    // Add each entry
    for (slist_t *curr = entries; curr != NULL; curr = curr->next) {
        // Build full path for stat
        char full_path[256];
        if (strcmp(path, "/") == 0) {
            snprintf(full_path, sizeof(full_path), "/%s", curr->data);
        } else {
            snprintf(full_path, sizeof(full_path), "%s/%s", path, curr->data);
        }
        
        rv = storage_stat(full_path, &st);
        if (rv == 0) {
            filler(buf, curr->data, &st, 0);
        }
    }
    
    slist_free(entries);
    
    printf("readdir(%s) -> 0\n", path);
    return 0;
}

/**
 * Create a filesystem object.
 *
 * Implementation for: man 2 mknod
 *
 * @param path Path for the new file.
 * @param mode File mode, types and permissions.
 * @param rdev Device number.
 *
 * @return 0 on success, negative error on fail.
 */
int nufs_mknod(const char *path, mode_t mode, dev_t rdev) {
    int rv = storage_mknod(path, mode);
    printf("mknod(%s, %04o) -> %d\n", path, mode, rv);
    return rv;
}

/**
 * Create a new directory.
 *
 * Implementation for: man 2 mkdir
 *
 * @param path Path for the new directory.
 * @param mode Directory permissions.
 *
 * @return 0 on success, negative error on fail.
 */
int nufs_mkdir(const char *path, mode_t mode) {
    int rv = storage_mknod(path, mode | S_IFDIR);
    printf("mkdir(%s) -> %d\n", path, rv);
    return rv;
}

/**
 * Delete a file.
 *
 * Implementation for: man 2 unlink
 *
 * @param path Path to the file to delete.
 *
 * @return 0 on success, negative error on fail.
 */
int nufs_unlink(const char *path) {
    int rv = storage_unlink(path);
    printf("unlink(%s) -> %d\n", path, rv);
    return rv;
}

/**
 * Create a hard link.
 *
 * Implementation for: man 2 link
 *
 * @param from Path to existing file.
 * @param to Path for new link.
 *
 * @return 0 on success, negative error on fail.
 */
int nufs_link(const char *from, const char *to) {
    int rv = storage_link(from, to);
    printf("link(%s => %s) -> %d\n", from, to, rv);
    return rv;
}

/**
 * Remove a directory.
 *
 * Implementation for: man 2 rmdir
 * Only succeeds if directory is empty.
 *
 * @param path Path to the directory to remove.
 *
 * @return 0 on success, -ENOTEMPTY if not empty, other errors.
 */
int nufs_rmdir(const char *path) {
    // Check if directory is empty
    slist_t *entries = storage_list(path);
    if (entries != NULL) {
        slist_free(entries);
        printf("rmdir(%s) -> -ENOTEMPTY\n", path);
        return -ENOTEMPTY;
    }
    
    int rv = storage_unlink(path);
    printf("rmdir(%s) -> %d\n", path, rv);
    return rv;
}

/**
 * Rename or move a file/directory.
 *
 * Implementation for: man 2 rename
 *
 * @param from Current path.
 * @param to New path.
 *
 * @return 0 on success, negative error on fail.
 */
int nufs_rename(const char *from, const char *to) {
    int rv = storage_rename(from, to);
    printf("rename(%s => %s) -> %d\n", from, to, rv);
    return rv;
}

/**
 * Change file permissions.
 *
 * Implementation for: man 2 chmod
 *
 * @param path Path to the file.
 * @param mode New permissions.
 *
 * @return 0 on success, negative error on fail.
 */
int nufs_chmod(const char *path, mode_t mode) {
    int rv = storage_chmod(path, mode);
    printf("chmod(%s, %04o) -> %d\n", path, mode, rv);
    return rv;
}

/**
 * Truncate a file to the specified size.
 *
 * Implementation for: man 2 truncate
 *
 * @param path Path to the file.
 * @param size New size in bytes.
 *
 * @return 0 on success, negative error on fail.
 */
int nufs_truncate(const char *path, off_t size) {
    int rv = storage_truncate(path, size);
    printf("truncate(%s, %ld bytes) -> %d\n", path, size, rv);
    return rv;
}

/**
 * Open a file.
 *
 * FUSE doesn't require maintaining state for open files,
 * so this just checks if the file exists.
 *
 * @param path Path to the file.
 * @param fi File info.
 *
 * @return 0 if file exists, -ENOENT otherwise.
 */
int nufs_open(const char *path, struct fuse_file_info *fi) {
    struct stat st;
    int rv = storage_stat(path, &st);
    printf("open(%s) -> %d\n", path, rv);
    return rv;
}

/**
 * Read data from a file.
 *
 * Implementation for: man 2 read
 *
 * @param path Path to the file.
 * @param buf Buffer to read into.
 * @param size Maximum bytes to read.
 * @param offset Byte offset to start reading from.
 * @param fi File info.
 *
 * @return Number of bytes read, or negative error code.
 */
int nufs_read(const char *path, char *buf, size_t size, off_t offset,
              struct fuse_file_info *fi) {
    int rv = storage_read(path, buf, size, offset);
    printf("read(%s, %ld bytes, @+%ld) -> %d\n", path, size, offset, rv);
    return rv;
}

/**
 * Write data to a file.
 *
 * Implementation for: man 2 write
 *
 * @param path Path to the file.
 * @param buf Buffer containing data to write.
 * @param size Number of bytes to write.
 * @param offset Byte offset to start writing at.
 * @param fi File info.
 *
 * @return Number of bytes written, or negative error code.
 */
int nufs_write(const char *path, const char *buf, size_t size, off_t offset,
               struct fuse_file_info *fi) {
    int rv = storage_write(path, buf, size, offset);
    printf("write(%s, %ld bytes, @+%ld) -> %d\n", path, size, offset, rv);
    return rv;
}

/**
 * Update file access and modification times.
 *
 * Implementation for: man 2 utimensat
 *
 * @param path Path to the file.
 * @param ts Array of two timespec: [0] = atime, [1] = mtime.
 *
 * @return 0 on success, negative error on fail.
 */
int nufs_utimens(const char *path, const struct timespec ts[2]) {
    int rv = storage_set_time(path, ts);
    printf("utimens(%s, [%ld, %ld; %ld %ld]) -> %d\n", 
           path, ts[0].tv_sec, ts[0].tv_nsec, ts[1].tv_sec, ts[1].tv_nsec, rv);
    return rv;
}

/**
 * Handle ioctl requests.
 *
 * @return -1
 */
int nufs_ioctl(const char *path, int cmd, void *arg, struct fuse_file_info *fi,
               unsigned int flags, void *data) {
    int rv = -1;
    printf("ioctl(%s, %d, ...) -> %d\n", path, cmd, rv);
    return rv;
}

/**
 * Initialize FUSE operations structure.
 *
 * Sets up the function pointers for all supported FUSE callbacks.
 *
 * @param ops Pointer to fuse_operations structure to initialize.
 */
void nufs_init_ops(struct fuse_operations *ops) {
    memset(ops, 0, sizeof(struct fuse_operations));
    ops->access = nufs_access;
    ops->getattr = nufs_getattr;
    ops->readdir = nufs_readdir;
    ops->mknod = nufs_mknod;
    ops->mkdir = nufs_mkdir;
    ops->link = nufs_link;
    ops->unlink = nufs_unlink;
    ops->rmdir = nufs_rmdir;
    ops->rename = nufs_rename;
    ops->chmod = nufs_chmod;
    ops->truncate = nufs_truncate;
    ops->open = nufs_open;
    ops->read = nufs_read;
    ops->write = nufs_write;
    ops->utimens = nufs_utimens;
    ops->ioctl = nufs_ioctl;
}

struct fuse_operations nufs_ops;

/**
 * Main entry point for the NUFS filesystem.
 *
 * Parses command line arguments, initializes storage, and starts FUSE.
 *
 * Usage: ./nufs [fuse_options] <mount_point> <disk_image>
 *
 * @param argc Argument count.
 * @param argv Argument vector.
 *
 * @return Exit code from fuse_main.
 */
int main(int argc, char *argv[]) {
    assert(argc > 2 && argc < 6);
    
    // Last argument is the disk image path
    const char *disk_image = argv[--argc];
    printf("mounting %s as data file\n", disk_image);
    
    // Initialize storage with disk image
    storage_init(disk_image);
    
    // Set up FUSE operations
    nufs_init_ops(&nufs_ops);
    
    // Start FUSE
    return fuse_main(argc, argv, &nufs_ops, NULL);
}
