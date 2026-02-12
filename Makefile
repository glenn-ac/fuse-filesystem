# NUFS Filesystem Makefile
#
# Builds the NUFS FUSE filesystem from source files in the current
# directory and the helpers/ subdirectory.

# Source files (current directory)
SRCS := nufs.c storage.c inode.c directory.c

# Helper source files
HELPER_SRCS := helpers/blocks.c helpers/bitmap.c helpers/slist.c

# All sources
ALL_SRCS := $(SRCS) $(HELPER_SRCS)

# Object files
OBJS := $(SRCS:.c=.o)
HELPER_OBJS := $(HELPER_SRCS:.c=.o)
ALL_OBJS := $(OBJS) $(HELPER_OBJS)

# Header files
HDRS := $(wildcard *.h) $(wildcard helpers/*.h)

# Compiler flags
CFLAGS := -g -Wall `pkg-config fuse --cflags`
LDLIBS := `pkg-config fuse --libs`

# Main target
nufs: $(ALL_OBJS)
	gcc $(CFLAGS) -o $@ $^ $(LDLIBS)

# Compile source files in current directory
%.o: %.c $(HDRS)
	gcc $(CFLAGS) -c -o $@ $<

# Compile helper source files
helpers/%.o: helpers/%.c $(HDRS)
	gcc $(CFLAGS) -c -o $@ $<

# Clean build artifacts
clean: unmount
	rm -f nufs *.o helpers/*.o test.log data.nufs
	rmdir mnt || true

# Mount the filesystem
mount: nufs
	mkdir -p mnt || true
	./nufs -s -f mnt data.nufs

# Unmount the filesystem
unmount:
	fusermount -u mnt || true

# Run tests
test: nufs
	perl test.pl

# Debug with GDB
gdb: nufs
	mkdir -p mnt || true
	gdb --args ./nufs -s -f mnt data.nufs

.PHONY: clean mount unmount test gdb
