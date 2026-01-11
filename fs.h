#ifndef FS_H
#define FS_H

/*
    Simple File System Interface
    ----------------------------
    This header defines the public API for the simple file system operations.
    It supports creating, deleting, reading, and writing files, as well as 
    formatting and mounting the file system.
*/

// Debugging: Dumps information about the superblock, inodes, and blocks.
void fs_debug();

// Formats the disk, clearing all data and setting up the superblock.
int fs_format();

// Mounts the file system from the disk. 
// Reads the superblock and builds the free block bitmap.
int fs_mount();

// Creates a new empty file and returns its inode number (inumber).
int fs_create();

// Deletes the file specified by the inumber. 
// Frees the inode and its associated data blocks.
int fs_delete(int inumber);

// Returns the logical size of the file specified by the inumber.
int fs_getsize(int inumber);

// Reads data from a file.
// inumber: The file to read from.
// data: Buffer to store read data.
// length: Number of bytes to read.
// offset: Byte offset in the file to start reading from.
int fs_read(int inumber, char *data, int length, int offset);

// Writes data to a file.
// inumber: The file to write to.
// data: Buffer containing data to write.
// length: Number of bytes to write.
// offset: Byte offset in the file to start writing to.
int fs_write(int inumber, const char *data, int length, int offset);

#endif