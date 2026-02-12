# FUSE Filesystem

A custom userspace filesystem built with [FUSE](https://github.com/libfuse/libfuse) (Filesystem in Userspace) in C. Implements a fully functional filesystem stored in a 1MB disk image with support for:

- File creation, reading, writing, and deletion
- Nested directories (mkdir, rmdir, listing)
- File renaming and moving between directories
- Hard links
- File truncation and timestamps
- Indirect block pointers for large files (up to 800KB+)

## Architecture

- **Block layer** (`helpers/blocks.c`) — mmaps a 1MB disk image into 256 × 4KB blocks
- **Bitmap allocator** (`helpers/bitmap.c`) — tracks free blocks and inodes
- **Inode system** (`inode.c`) — manages file metadata with direct and indirect block pointers
- **Directory layer** (`directory.c`) — maps filenames to inodes within directory blocks
- **Storage layer** (`storage.c`) — bridges FUSE callbacks to the inode/directory system
- **FUSE driver** (`nufs.c`) — implements the FUSE callback interface

## Running

This project is designed to run in a **Linux environment** (e.g., GitHub Codespaces, Docker, or a Linux VM). FUSE requires kernel support that is not natively available on macOS or Windows.

### Quick Start with GitHub Codespaces

1. Click **Code → Codespaces → Create codespace on main** from the GitHub repo
2. In the terminal:

sudo apt-get install -y libfuse-dev pkg-config libtest-simple-perl
make mount

## Unmount and Remove Build Artifacts
make unmount   # Unmount
make clean     # Remove build artifacts