# Simple File System Project

A simulation of a basic file system implemented in C. This project demonstrates core operating system concepts including inodes, blocks, bitmaps, and file operations.

## Overview
This project implements a simple file system (SimpleFS) that runs on top of an emulated disk. It handles:
- **Superblock management**: Filesystem metadata.
- **Inode management**: File metadata tracking.
- **Free block management**: Using a bitmap.
- **Basic operations**: Create, delete, read, write, and more.

## Building the Project
To compile the project, run:
```bash
make
```
This generates the `simplefs` executable.

## Running the Shell
The `simplefs` program acts as a shell to interact with your file system.

Usage:
```bash
./simplefs <diskfile> <nblocks>
```
- `<diskfile>`: The name of the file to use as the emulated disk (will be created if it doesn't exist).
- `<nblocks>`: The number of blocks to allocate for the disk.

### Example
```bash
./simplefs mydisk 200
```

## Shell Commands
Once inside the `simplefs>` shell, you can use the following commands:
- `format`: Format the disk (must do this first).
- `mount`: Mount the file system (must do this after formatting).
- `create`: Create a new file (returns inumber).
- `delete <inumber>`: Delete a file.
- `cat <inumber>`: Display file contents.
- `copyin <filename> <inumber>`: Copy a file from the host OS into SimpleFS.
- `copyout <inumber> <filename>`: Copy a file from SimpleFS to the host OS.
- `stat <inumber>`: Show file statistics.
- `debug`: Print file system state (superuser block, inodes).
- `quit`: Exit the shell.

## Example Session
```plaintext
simplefs> format
disk formatted.
simplefs> mount
disk mounted.
simplefs> create
created inode 1
simplefs> copyin test.txt 1
wrote 512 bytes to inode 1
simplefs> debug
... (prints fs state) ...
simplefs> quit
```
