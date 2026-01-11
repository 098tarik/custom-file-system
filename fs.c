
#include "fs.h"
#include "disk.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

/*
    Simple File System Implementation
    ---------------------------------
    This file implements the simple file system 
    with a superblock, inodes, bit map for free 
    blocks, and data blocks.
*/

// File System Constants
#define DISK_BLOCK_SIZE 4096      // Size of a single block in bytes
#define FS_MAGIC 0xf0f03410       // Magic number to identify the file system
#define INODES_PER_BLOCK 128      // Number of inodes fitting in one block
#define POINTERS_PER_INODE 5      // Number of direct pointers in an inode
#define POINTERS_PER_BLOCK 1024   // Number of pointers in an indirect block

// Global variables for file system state
int fs_mounted = 0;      // Flag to check if FS is mounted
int *bitmap;             // Bitmap to track free/used data blocks
int num_inode_blocks;    // Number of blocks dedicated to inodes

// Superblock Structure: Contains metadata about the filesystem
struct fs_superblock
{
    int magic;          // Magic number (FS_MAGIC)
    int nblocks;        // Total number of blocks in the disk
    int ninodeblocks;   // Number of blocks reserved for inodes
    int ninodes;        // Total number of inodes
};

// Inode Structure: Metadata for a single file
struct fs_inode
{
    int isvalid;                    // Flag: 1 if inode is in use, 0 otherwise
    int size;                       // Size of the file in bytes
    int direct[POINTERS_PER_INODE]; // Direct pointers to data blocks
    int indirect;                   // Indirect pointer (points to a block of pointers)
};

// Union for Block: Represents a generic block on disk
// Can be interpreted as a superblock, array of inodes, array of pointers, or raw data
union fs_block
{
    struct fs_superblock super;
    struct fs_inode inode[INODES_PER_BLOCK];
    int pointers[POINTERS_PER_BLOCK];
    char data[DISK_BLOCK_SIZE];
};

// Helper: Allocates a new block by searching the bitmap for a free block
int allocate_new_block(int blocks)
{
    for (int i = 0; i < blocks; i++)
    {
        if (!bitmap[i])
        {
            bitmap[i] = 1; // Mark block as used
            return i;
        }
    }

    return 0; // No free block found
}

// Helper: Creates a new bitmap by scanning the disk content
// Used during mount to verify the filesystem and rebuild the free block map
int create_new_bitmap()
{

    union fs_block block;

    union fs_block indirect_block;

    // Begin processing of the new bitmap
    for (int i = 0; i < disk_size(); i++)
    {
        // Read info for new filesystem
        disk_read(i, block.data);

        if (i == 0)
        { // Check if superblock and set it
            if (block.super.magic == FS_MAGIC)
            {
                bitmap[0] = 1;
            }
            else
            {
                return -1;
            }
        }
        // set inode blocks as valid and set the data blocks that
        // they point to as valid
        else if (i <= num_inode_blocks)
        {
            bitmap[i] = 1;

            // Go through each inode in the inode block
            for (int j = 0; j < INODES_PER_BLOCK; j++)
            {
                // Check if the inode is valid
                if (block.inode[j].isvalid)
                {

                    // Check the direct pointers
                    for (int k = 0; k < POINTERS_PER_INODE; k++)
                    {
                        if (block.inode[j].direct[k])
                        {
                            bitmap[block.inode[j].direct[k]] = 1;
                        }
                    }

                    // Check the indirect pointer
                    if (block.inode[j].indirect)
                    {
                        // Read the indirect pointer and check the indirect block
                        disk_read(block.inode[j].indirect, indirect_block.data);

                        bitmap[block.inode[j].indirect] = 1;

                        // Check pointers on indirect block
                        for (int l = 0; l < POINTERS_PER_BLOCK; l++)
                        {
                            if (indirect_block.pointers[l])
                            {
                                bitmap[indirect_block.pointers[l]] = 1;
                            }
                        }
                    }
                }
            }
        }
    }

    return 1;
}

void print_inode(struct fs_inode *current_inode, int inode_block, int block_offset)
{
    // Counter for number of direct blocks
    int direct_blocks = 0;

    // Print the inode number and size
    printf("inode %d:\n", (inode_block - 1) * INODES_PER_BLOCK + block_offset);
    printf("    size: %d bytes\n", current_inode->size);

    // Search for direct blocks to report
    for (int i = 0; i < POINTERS_PER_INODE; i++)
    {
        // If direct block is found, report information
        if (current_inode->direct[i])
        {
            direct_blocks++;

            if (direct_blocks == 1)
            {
                printf("    direct blocks:");
            }

            // Lists direct blocks
            printf(" %d", current_inode->direct[i]);
        }

        // Determine if more spacing is needed
        if (i == POINTERS_PER_INODE - 1 && direct_blocks)
        {
            printf("\n");
        }
    }

    // Checks if inode has an indirect block
    if (current_inode->indirect)
    {
        union fs_block indirect_block;

        // Reads information about indirect block
        disk_read(current_inode->indirect, indirect_block.data);

        // Print information about indirect block
        printf("    indirect block: %d\n", current_inode->indirect);
        printf("    indirect data blocks:");

        for (int j = 0; j < POINTERS_PER_BLOCK; j++)
        {
            // Lists indirect data blocks
            if (indirect_block.pointers[j])
            {
                printf(" %d", indirect_block.pointers[j]);
            }
        }

        printf("\n");
    }
}

int set_inode_blocks()
{
    int inode_blocks;

    // Ensure that at least ten percent of the blocks are reserved for inodes
    if (disk_size() % 10 == 0)
    {
        inode_blocks = disk_size() / 10;
    }
    else
    {
        inode_blocks = (disk_size() / 10) + 1;
    }

    return inode_blocks;
}

void destroy_data(int inode_blocks)
{
    union fs_block block;

    for (int i = 1; i <= inode_blocks; i++)
    {
        // Read inode block
        disk_read(i, block.data);

        // Traverse each inode in the inode block
        for (int j = 0; j < INODES_PER_BLOCK; j++)
        {
            // Make each inode invalid
            block.inode[j].isvalid = 0;
        }

        // Write the destroyed inode block back to the disk
        disk_write(i, block.data);
    }
}

void fs_debug()
{
    union fs_block block;
    disk_read(0, block.data);

    printf("superblock:\n");
    printf("    %d blocks\n", block.super.nblocks);
    printf("    %d inode blocks\n", block.super.ninodeblocks);
    printf("    %d inodes\n", block.super.ninodes);

    // Traverse each inode block
    for (int i = 1; i <= block.super.ninodeblocks; i++)
    {
        // Read inode block
        disk_read(i, block.data);

        // Traverse each inode in the inode block
        for (int j = 0; j < INODES_PER_BLOCK; j++)
        {
            // If inode is valid (has info), print it out
            if (block.inode[j].isvalid)
            {
                print_inode(&(block.inode[j]), i, j);
            }
        }
    }
}

// fs_format: Prepares the disk for use by the file system.
// It initializes the superblock, reserves space for inodes, and clears regular data blocks.
int fs_format()
{
    // If filesystem is already mounted, return an error
    if (fs_mounted)
    {
        return 0;
    }

    union fs_block block;

    // Create each element of the super block
    block.super.magic = FS_MAGIC;
    block.super.nblocks = disk_size();
    block.super.ninodeblocks = set_inode_blocks();
    block.super.ninodes = block.super.ninodeblocks * INODES_PER_BLOCK;

    // Destroy any existing data in the filesystem
    destroy_data(block.super.ninodeblocks);
    disk_write(0, block.data);

    // Formatted successfully
    return 1;
}

// fs_mount: Mounts the file system.
// It reads the superblock to verify the filesystem and builds the free block bitmap.
int fs_mount()
{
    union fs_block block;
    disk_read(0, block.data);

    // Cannot mount on top of an another filesystem
    if (block.super.magic != FS_MAGIC)
    {
        return 0;
    }

    // Allocate space for the new free block bitmap
    bitmap = calloc(block.super.nblocks, sizeof(int));

    // Allocation failed
    if (!bitmap)
    {
        return 0;
    }

    // Set new values for the system and mark as mounted
    num_inode_blocks = block.super.ninodeblocks;
    fs_mounted = 1;

    // Creates new free block bitmap
    int rc = create_new_bitmap();
    if (rc == -1) { 
        return 0;
    }

    // Mounted successfully
    return 1;
}

// fs_create: Creates a new empty file.
// Scans inode blocks to find a free inode, initializes it, and returns its inode number.
int fs_create()
{
    // Check to see if a disk is mounted
    if (!fs_mounted)
    {
        return 0;
    }

    union fs_block block;
    // Use the global num_inode_blocks which was set during mount
    // to avoid overwriting superblock data in the buffer during iteration

    for (int k = 1; k <= num_inode_blocks; k++)
    {
        // Read inode block
        disk_read(k, block.data);
        struct fs_inode inode;

        // Traverse each inode in the inode block
        for (int j = 1; j < INODES_PER_BLOCK; j++)
        {
            inode = block.inode[j];

            // If inode is invalid. insert valid inode
            if (!inode.isvalid)
            {
                int free_inode = (k - 1) * INODES_PER_BLOCK + j;

                inode.isvalid = 1;
                inode.size = 0;

                // Setting direct pointers to 0
                for (int i = 0; i < POINTERS_PER_INODE; i++)
                {
                    inode.direct[i] = 0;
                }
                
                inode.indirect = 0;

                block.inode[j] = inode;

                disk_write(k, block.data);
                
                // Return immediately after creating the inode
                return free_inode;
            }
        }
    }
    // No free inodes found
    return 0;
}

// fs_delete: Deletes a file.
// Frees all data blocks associated with the inode and marks the inode as invalid.
int fs_delete(int inumber)
{
    // Check to see if a disk is mounted
    if (!fs_mounted)
    {
        return 0;
    }

    union fs_block super_block;
    disk_read(0, super_block.data);

    if (inumber > super_block.super.ninodes || inumber <= 0)
    {
        return 0;
    }

    union fs_block block;
    // Read inode block
    disk_read(((inumber / INODES_PER_BLOCK) + 1), block.data);

    // if inode doesn't exist, return 0
    if (!block.inode[inumber % INODES_PER_BLOCK].isvalid)
    {
        return 0;
    }

    // for each direct block mapping, if it has a value, free it
    for (int i = 0; i < POINTERS_PER_INODE; i++)
    {

        if (block.inode[inumber % INODES_PER_BLOCK].direct[i])
        {
            // set the bitmap entry for the pointed-to block to 0
            bitmap[block.inode[inumber % INODES_PER_BLOCK].direct[i]] = 0;
            // set the pointer to 0
            block.inode[inumber % INODES_PER_BLOCK].direct[i] = 0;
        }
    }
    // if there is an indirect block mapping, free the data blocks mapped from the indirect block

    if (block.inode[inumber % INODES_PER_BLOCK].indirect)
    {
        union fs_block indirect_block;
        disk_read(block.inode[inumber % INODES_PER_BLOCK].indirect, indirect_block.data);

        for (int i = 0; i < POINTERS_PER_BLOCK; i++)
        {
            if (indirect_block.pointers[i]);
            {
                // set the bitmap entry for the pointed-to block to 0
                bitmap[indirect_block.pointers[i]] = 0;
                // set the pointer to 0
                indirect_block.pointers[i] = 0;
            }
        }
        disk_write(block.inode[inumber % INODES_PER_BLOCK].indirect, indirect_block.data);
    }
    // set the indirect pointer to zero
    block.inode[inumber % INODES_PER_BLOCK].indirect = 0;

    // set valid bit to 0
    block.inode[inumber % INODES_PER_BLOCK].isvalid = 0;
    block.inode[inumber % INODES_PER_BLOCK].size = 0;
    // write
    disk_write(((inumber / INODES_PER_BLOCK) + 1), block.data);

    return 1;
}

// fs_getsize: Returns the size of the file in bytes.
// Retrieves the inode for the given inumber and returns its size field.
int fs_getsize(int inumber)
{
    union fs_block block;
    disk_read(0, block.data);

    // Check if inumber is 0, which is an invalid inumber.
    if (inumber <= 0)
    {
        return -1;
    }
    // Find correct inode block
    int index = (inumber + INODES_PER_BLOCK) / INODES_PER_BLOCK;

    // Check if inode block is in limits
    if (index > block.super.ninodeblocks)
    {
        return -1;
    }

    // Read inode from inode block
    disk_read(index, block.data);
    struct fs_inode inode = block.inode[inumber % INODES_PER_BLOCK];

    // Check if valid inode; if inode is valid, return the size
    if (inode.isvalid)
    {
        return inode.size;
    }

    // Inode was invalid and return error
    return -1;
}

// fs_read: Reads data from a file.
// Handles reading from direct blocks and the indirect block to gather file content.
int fs_read(int inumber, char *data, int length, int offset)
{
    // Check to see if a filesystem is mounted
    if (!fs_mounted)
    {
        return 0;
    }

    // Check to see if a valid inumber is passed
    if (inumber <= 0)
    {
        return 0;
    }

    int pointer_count, is_direct_block, bytes_left, bytes_read = 0;

    union fs_block block, indirect_block;
    struct fs_inode inode;

    char loop_data[4096] = "";
    char total_data[16384] = "";

    // Determine the inode offset as well as the inode block and pointer offset
    int inode_offset = inumber % INODES_PER_BLOCK;
    int block_index = inumber / INODES_PER_BLOCK + 1;
    int pointer_offset = offset / 4096;

    // Read from the inodes block
    disk_read(block_index, block.data);
    inode = block.inode[inode_offset];
    int inode_size = inode.size;

    // Check to make sure inode is valid and has a reasonable size
    if ((!inode.isvalid) || !inode_size)
    {
        return 0;
    }

    // Determine how many bytes can/need to be read
    if ((inode_size - offset) < length)
    {
        bytes_left = inode_size - offset;
    }
    else
    {
        bytes_left = length;
    }

    // Traverse through each direct pointer in the inode
    for (int i = pointer_offset; i < POINTERS_PER_INODE; i++)
    {
        is_direct_block = inode.direct[i];

        // If direct block exists, read a piece of data and copy it. Recalculate how much has been read
        if (is_direct_block)
        {
            disk_read(is_direct_block, *(&loop_data));
            strcat(*(&total_data), *(&loop_data));

            if ((bytes_left - bytes_read) < 4096)
            {
                bytes_read += bytes_left - bytes_read;
            }
            else
            {
                bytes_read += 4096;
            }

            if (bytes_read >= bytes_left)
            {
                strcpy(data, total_data);
                return bytes_read;
            }
        }
    }

    // Traverse through each indirect pointer in the inode if it exists
    if (inode.indirect)
    {
        disk_read(inode.indirect, indirect_block.data);

        if (pointer_offset < 5)
        {
            pointer_count = 0;
        }
        else
        {
            pointer_count = pointer_offset - 5;
        }

        // Start looking from pointer offset
        for (int j = pointer_count; j < POINTERS_PER_BLOCK; j++)
        {
            if (indirect_block.pointers[j])
            {
                disk_read(indirect_block.pointers[j], *(&loop_data));
                strcat(*(&total_data), *(&loop_data));

                if ((bytes_left - bytes_read) < 4096)
                {
                    bytes_read += bytes_left - bytes_read;
                }
                else
                {
                    bytes_read += 4096;
                }

                if (bytes_read >= bytes_left)
                {
                    strcpy(data, total_data);
                    return bytes_read;
                }
            }
        }
    }

    return bytes_read;
}

// fs_write: Writes data to a file.
// Handles allocating new blocks as needed (both direct and indirect).
// Updates the file size in the inode.
int fs_write(int inumber, const char *data, int length, int offset)
{
    // Check to see if a filesystem is mounted
    if (!fs_mounted)
    {
        return 0;
    }

    // Check to see if a valid inumber is passed
    if (inumber <= 0)
    {
        return 0;
    }

    int pointer_count, new_block, bytes_left, bytes_written = 0;
    union fs_block block, indirect_block, super_block;

    char total_data[16384] = "";
    strcpy(total_data, data);

    disk_read(0, super_block.data);

    // Determine the inode offset as well as the inode block and pointer offset
    int inode_offset = inumber % INODES_PER_BLOCK;
    int block_index = inumber / INODES_PER_BLOCK + 1;
    int pointer_offset = offset / 4096;

    // Read from the inodes block
    disk_read(block_index, block.data);

    int inode_size = (POINTERS_PER_INODE + POINTERS_PER_BLOCK) * 4096;

    // Determine how many bytes can/need to be written
    if ((inode_size - offset) < length)
    {
        bytes_left = inode_size - offset;
    }
    else
    {
        bytes_left = length;
    }

    // Check if inode is valid
    if (!block.inode[inode_offset].isvalid)
    {
        return 0;
    }

    // If the offset is 0 at the start, reset direct and indirect pointers to 0
    if (offset == 0)
    {
        //Iterate through pointers of direct block
        for (int x = 0; x < POINTERS_PER_INODE; x++)
        {
            if (block.inode[inode_offset].direct[x] <= 0)
            {
                continue;
            }

            bitmap[block.inode[inode_offset].direct[x]] = 0;
            block.inode[inode_offset].direct[x] = 0;
        }

        if (block.inode[inode_offset].indirect > 0)
        {
            union fs_block indirect_block;
            disk_read(block.inode[inode_offset].indirect, indirect_block.data);

            // Iterate through pointers of indirect block
            for (int y = 0; y < POINTERS_PER_BLOCK; y++)
            {
                if (indirect_block.pointers[y] <= 0)
                {
                    continue;
                }

                bitmap[indirect_block.pointers[y]] = 0;
                indirect_block.pointers[y] = 0;
            }

            disk_write(block.inode[inode_offset].indirect, indirect_block.data);

            bitmap[block.inode[inode_offset].indirect] = 0;
            block.inode[inode_offset].indirect = 0;
        }

        disk_write(block_index, block.data);
    }

    // Traverse through each direct pointer in the inode
    for (int i = pointer_offset; i < POINTERS_PER_INODE; i++)
    {
        new_block = allocate_new_block(super_block.super.nblocks);

        // If the disk is full, there are no more blocks left
        if (!new_block)
        {
            block.inode[inode_offset].size = offset + bytes_written;
            disk_write(block_index, block.data);

            return bytes_written;
        }

        block.inode[inode_offset].direct[i] = new_block;
        disk_write(block.inode[inode_offset].direct[i], total_data);

        // Write a piece of data and copy it. Recalculate how much has been read
        if ((bytes_left - bytes_written) < DISK_BLOCK_SIZE)
        {
            bytes_written += bytes_left - bytes_written;
        }
        else
        {
            bytes_written += DISK_BLOCK_SIZE;
        }

        strcpy(total_data, &data[bytes_written]);

        // Check to see if too many bytes were written
        if (bytes_written >= bytes_left)
        {
            block.inode[inode_offset].size = offset + bytes_written;
            disk_write(block_index, block.data);

            return bytes_written;
        }
    }

    // Check if you need to create an indirect block
    if (!block.inode[inode_offset].indirect)
    {
        block.inode[inode_offset].indirect = allocate_new_block(super_block.super.nblocks);
        disk_write(block.inode[inode_offset].indirect, indirect_block.data);
    }

    // Traverse through all the pointers in the indirect block
    if (pointer_offset < 5)
    {
        pointer_count = 0;
    }
    else
    {
        pointer_count = pointer_offset - 5;
    }

    for (int j = pointer_count; j < POINTERS_PER_BLOCK; j++)
    {
        new_block = allocate_new_block(super_block.super.nblocks);

        // If the disk is full, there are no more blocks left
        if (!new_block)
        {
            block.inode[inode_offset].size = offset + bytes_written;
            disk_write(block.inode[inode_offset].indirect, indirect_block.data);

            disk_write(block_index, block.data);

            return bytes_written;
        }

        indirect_block.pointers[j] = new_block;
        disk_write(indirect_block.pointers[j], total_data);

        // Write a piece of data and copy it. Recalculate how much has been read
        if ((bytes_left - bytes_written) < DISK_BLOCK_SIZE)
        {
            bytes_written += bytes_left - bytes_written;
        }
        else
        {
            bytes_written += DISK_BLOCK_SIZE;
        }

        strcpy(total_data, &data[bytes_written]);

        // Check to see if too many bytes were written
        if (bytes_written >= bytes_left)
        {
            block.inode[inode_offset].size = offset + bytes_written;
            disk_write(block.inode[inode_offset].indirect, indirect_block.data);

            disk_write(block_index, block.data);

            // Iterate through pointers of indirect block
            for (int i = 0; i < POINTERS_PER_BLOCK; i++)
            {
                if (indirect_block.pointers[i] <= 0)
                {
                    continue;
                }
            }

            return bytes_written;
        }
    }

    return bytes_written;
}