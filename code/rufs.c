/*
 *  Copyright (C) 2023 CS416 Rutgers CS
 *	Tiny File System
 *	File:	rufs.c
 *
 */

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/time.h>
#include <libgen.h>
#include <limits.h>

#include "block.h"
#include "rufs.h"

char diskfile_path[PATH_MAX];

#define SUPERBLOCK_NUM 0

// Declare your in-memory data structures here

bitmap_t* inode_bmap;
bitmap_t* data_bmap;
struct superblock* superblk;

/* 
 * Get available inode number from bitmap
 */
int get_avail_ino() {

	// Step 1: Read inode bitmap from disk
	
	// Step 2: Traverse inode bitmap to find an available slot

	// Step 3: Update inode bitmap and write to disk 

	// Reread in case of issues
	bio_read(superblk->i_bitmap_blk, inode_bmap);

	for(int i = 0; i < BLOCK_SIZE; i++) {
		if(get_bitmap(inode_bmap, i) == 0) {
			set_bitmap(inode_bmap, i);
			bio_write(superblk->i_bitmap_blk, inode_bmap);
			return i;
		}
	}

	// Not found
	return -1;
}

/* 
 * Get available data block number from bitmap
 */
int get_avail_blkno() {

	// Step 1: Read data block bitmap from disk
	
	// Step 2: Traverse data block bitmap to find an available slot

	// Step 3: Update data block bitmap and write to disk 

	// Reread in case of issues
	bio_read(superblk->d_bitmap_blk, data_bmap);

	for(int i = 0; i < MAX_DNUM; i++) {
		if(get_bitmap(data_bmap, i) == 0) {
			set_bitmap(data_bmap, i);
			bio_write(superblk->i_bitmap_blk, data_bmap);
			return i;
		}
	}

	// Not found
	return -1;
}

/* 
 * inode operations
 */
int readi(uint16_t ino, struct inode *inode) {

  // Step 1: Get the inode's on-disk block number

  // Step 2: Get offset of the inode in the inode on-disk block

  // Step 3: Read the block from disk and then copy into inode structure

	// Find the block of the inode
	int block_no = ino / 16;
	// Offset is the number of inodes, multiply by the size of each inode.
	int offset = ino % 16;
	block_no += superblk->i_start_blk;

	// WE CAN GO BYTE FOR BYTE
	char *block = malloc(BLOCK_SIZE);

	if(bio_read(block_no, block) < 0) {
		printf("Read Error within readi");
		return -1;
	}

	struct inode *ptr_to_inode = *block + (offset * sizeof(inode));

	memcpy(inode, ptr_to_inode, sizeof(inode));

	free(block);
	return 0;
}

int writei(uint16_t ino, struct inode *inode) {

	// Step 1: Get the block number where this inode resides on disk
	
	// Step 2: Get the offset in the block where this inode resides on disk

	// Step 3: Write inode to disk 

	// Find the block of the inode
	int block_no = ino / 16;
	block_no += superblk->i_start_blk;

	// Offset is the number of inodes, multiply by the size of each inode.
	int offset = ino % 16;

	// WE CAN GO BYTE FOR BYTE
	char *block = malloc(BLOCK_SIZE);

	if(bio_read(block_no, block) < 0) {
		printf("Read Error within writei");
		return -1;
	}

	struct inode *ptr_to_inode = *block + (offset * sizeof(inode));

	// Copy the inode to the place where it belongs in the block
	memcpy(ptr_to_inode, inode, sizeof(inode));

	// Write the entire block back to the disk.
	bio_write(block_no, block);

	free(block);

	return 0;
}


/* 
 * directory operations
 */
int dir_find(uint16_t ino, const char *fname, size_t name_len, struct dirent *dirent) {

  // Step 1: Call readi() to get the inode using ino (inode number of current directory)

  // Step 2: Get data block of current directory from inode

  // Step 3: Read directory's data block and check each directory entry.
  //If the name matches, then copy directory entry to dirent structure

  // Notes: Use strcmp()
	struct inode *dir_inode = malloc(sizeof(struct inode));

	if(readi(ino, dir_inode) < 0) {
		printf("Error finding inode inside dir_find");
		return -1;
	}

	// Get data block of current directory
	struct dirent *curr_dir_block = malloc(BLOCK_SIZE);
	// read through direct pointers?
	for(int ptr = 0; ptr < 16; ptr++) {
		
		// Dont exist
		if(!dir_inode->direct_ptr[ptr]) {
			free(dir_inode);
			return 0;
		}

		if(bio_read(dir_inode->direct_ptr[ptr], curr_dir_block) < 0) {
			printf("Error reading from disk within dir_find");
			free(dir_inode);
			return -1;
		}

		// Now I should have the data block. Read thru the whole thing for dir entries
		struct dirent *dir_ptr = curr_dir_block;
		for(int i = 0; i < (BLOCK_SIZE / sizeof(struct dirent)); i++, dir_ptr++) {
			if(!dir_ptr->valid) continue;
			else if(strcmp(dir_ptr->name, fname) == 0) {
				memcpy(dirent,dir_ptr, sizeof(struct dirent));
				free(curr_dir_block);
				free(dir_inode);
				return 1;
			}
		}

	}

	free(curr_dir_block);
	free(dir_inode);
	return 0;
}

int dir_add(struct inode dir_inode, uint16_t f_ino, const char *fname, size_t name_len) {

	// Step 1: Read dir_inode's data block and check each directory entry of dir_inode
	
	// Step 2: Check if fname (directory name) is already used in other entries

	// Step 3: Add directory entry in dir_inode's data block and write to disk

	// Allocate a new data block for this directory if it does not exist

	// Update directory inode

	// Write directory entry

	// Dont exist
	if(!dir_inode.direct_ptr[0]) {
		// Create a data block entry.
		int new_data_block = get_avail_blkno();
		dir_inode.direct_ptr[0] = new_data_block;
		writei(dir_inode.ino, &dir_inode);

		// Add the entry.
		struct dirent* new_dir = malloc(BLOCK_SIZE);

		new_dir[0] = (struct dirent) {
			.ino = f_ino,
			.valid = 1,
			.len = (uint16_t) strlen(fname)
		};
		strcpy(new_dir[0].name, fname);

		
		if(bio_write(new_data_block, new_dir) < 0) {
			printf("error writing to file from dir_add");
			free(new_dir);
			return -1;
		}
		free(new_dir);
		return 0;
	}

	// There are data blocks alr...
	// First try and find it...
	struct dirent searching_dir;
	if(dir_find(dir_inode.ino, fname, name_len, &searching_dir) == 1) {
		printf("File name already exists");
		return -1;
	}

	struct dirent *curr_dir_block = malloc(BLOCK_SIZE);
	// read through direct pointers?
	for(int ptr = 0; ptr < 16; ptr++) {
		
		if(bio_read(dir_inode.direct_ptr[ptr], curr_dir_block) < 0) {
			printf("Error reading from disk within dir_add");
			free(curr_dir_block);
			return -1;
		}

		// Now I should have the data block. Read thru the whole thing for dir entries
		struct dirent *dir_ptr = curr_dir_block;
		for(int i = 0; i < (BLOCK_SIZE / sizeof(struct dirent)); i++, dir_ptr++) {
			// Find a place i can insert. REMINDER that because i am searching for a invalid entry to insert into, i need to invalidate entries
			if(dir_ptr->valid) continue;
			else {
				// Add the entry.
				*dir_ptr = (struct dirent) {
					.ino = f_ino,
					.valid = 1,
					.len = (uint16_t) strlen(fname)
				};

				strcpy(dir_ptr->name, fname);

				// More stats I need to add to this for sure
				dir_inode.size += sizeof(struct dirent);

				if(writei(dir_inode.ino, &dir_inode) < 0) {
					printf("error writing to file from dir_add");
					free(curr_dir_block);

					return -1;
				}

				// Write the new block with the new directory to the disk
				if(bio_write(dir_inode.direct_ptr[ptr], curr_dir_block) < 0) {
					printf("Write error inside dir_add");
					free(curr_dir_block);
					return -1;
				}
				free(curr_dir_block);
				return 0;
			}
		}

	}
	free(curr_dir_block);
	return 0;
}

int dir_remove(struct inode dir_inode, const char *fname, size_t name_len) {

	// Step 1: Read dir_inode's data block and checks each directory entry of dir_inode
	
	// Step 2: Check if fname exist

	// Step 3: If exist, then remove it from dir_inode's data block and write to disk

	return 0;
}

/* 
 * namei operation
 */
int get_node_by_path(const char *path, uint16_t ino, struct inode *inode) {
	
	// Step 1: Resolve the path name, walk through path, and finally, find its inode.
	// Note: You could either implement it in a iterative way or recursive way

	// https://stackoverflow.com/questions/9210528/split-string-with-delimiters-in-c
	// Use strtok?
	char *path_cpy = strdup(path);

	// char* token = strtok(path_cpy, '/');

	char *token, *str, *str_cpy;

	str_cpy = str = strdup(path);

	struct dirent temp_dir;
	temp_dir.ino = 0; // Root
	while ((token = strsep(&str, "/"))) {
		if(dir_find(temp_dir.ino, token, strlen(token), &temp_dir) < 0) {
			free(str_cpy);
			return -1;
		}
	}
	// It should have found it by now
	if(temp_dir.valid) {
		if(readi(temp_dir.ino, inode) < 0) {
			printf("error reading inode block inside get_node_by_path");
			return -1;
		}
	}

	free(str_cpy);
	return 0;
}

/* 
 * Make file system
 */
int rufs_mkfs() {

	// Call dev_init() to initialize (Create) Diskfile

	// write superblock information

	// initialize inode bitmap

	// initialize data block bitmap

	// update bitmap information for root directory

	// update inode for root directory

	struct inode root_ino;
	struct dirent root_dir;


	dev_init(diskfile_path);


	// Create the superblock
	superblk = malloc(BLOCK_SIZE);

	*superblk = (struct superblock) {
		.magic_num = MAGIC_NUM,
		.max_inum = MAX_INUM,
		.max_dnum = MAX_DNUM,
		.i_bitmap_blk = 1,
		.d_bitmap_blk = 2,
		.i_start_blk = 3,
		.d_start_blk = 3 + (sizeof(struct inode) * MAX_INUM / BLOCK_SIZE)
	};

	// Write to disk
	bio_write(SUPERBLOCK_NUM, superblk);

	// Create bmaps
	inode_bmap = calloc(1, BLOCK_SIZE);
	data_bmap = calloc(1, BLOCK_SIZE);

	// Write bmaps
	bio_write(superblk->i_bitmap_blk, inode_bmap);
	bio_write(superblk->i_bitmap_blk, data_bmap);

	int root_ino_num = get_avail_ino();
	if(root_ino_num == -1) {
		printf("Error setting root inode, no available inode number");
		return -1;
	}

	// Set the bmap for the root directory.
	root_ino = (struct inode) {
		.ino = root_ino_num,
		.valid = 1,
		.size = 0,
		.type = 1,
		.direct_ptr[0] = superblk->d_start_blk
	};

	// Write the root inode to the inode block
	bio_write(superblk->i_start_blk, &root_ino);

	// int root_blk_num = get_avail_blkno();
	// if(root_blk_num == -1) {
	// 	printf("Error setting root inode, no available block number");
	// 	return -1;
	// }

	// // Set the directory entry for the root
	// root_dir = (struct dirent) {
	// 	.name = '/',
	// 	.len  = 1,
	// 	.ino = root_blk_num,
	// 	.valid = 1
	// };

	// bio_write(superblk->d_start_blk, &root_dir);

	// // Rewrite the direct path of the root dir, just in case
	// root_ino.direct_ptr[0] = root_blk_num;
	// bio_write(superblk->i_start_blk, &root_ino);

	return 0;
}


/* 
 * FUSE file operations
 */
static void *rufs_init(struct fuse_conn_info *conn) {

	// Step 1a: If disk file is not found, call mkfs

  // Step 1b: If disk file is found, just initialize in-memory data structures
  // and read superblock from disk

	// Open doesnt find it
	if(dev_open(diskfile_path) < 0) {
		rufs_mkfs();
		return NULL;
	}

	// Based on the slide organization?
	inode_bmap = malloc(BLOCK_SIZE);
	data_bmap = malloc(BLOCK_SIZE);
	superblk = malloc(BLOCK_SIZE);

	// Read each block into the data structures
	if(bio_read(SUPERBLOCK_NUM, superblk) < 0) {
		printf("File Read Error: Superblock");
		return NULL;
	}

	if(bio_read(superblk->i_bitmap_blk, inode_bmap) < 0) {
		printf("File Read Error: Inode Bmap Block");
		return NULL;
	}

	if(bio_read(superblk->d_start_blk, data_bmap) < 0) {
		printf("File Read Error: Data Bmap Block");
		return NULL;
	}


	return NULL;
}

static void rufs_destroy(void *userdata) {

	// Step 1: De-allocate in-memory data structures

	// Step 2: Close diskfile
	free(superblk);
	free(data_bmap);
	free(inode_bmap);
	dev_close();

}

static int rufs_getattr(const char *path, struct stat *stbuf) {

	// Step 1: call get_node_by_path() to get inode from path

	// Step 2: fill attribute of file into stbuf from inode

		stbuf->st_mode   = S_IFDIR | 0755;
		stbuf->st_nlink  = 2;
		time(&stbuf->st_mtime);

	return 0;
}

static int rufs_opendir(const char *path, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path

	// Step 2: If not find, return -1

	struct inode dir;

	int ret = get_node_by_path(path, 0, &dir);
	return ret;
}

static int rufs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path

	// Step 2: Read directory entries from its data blocks, and copy them to filler

	struct inode dir;

	if(get_node_by_path(path, 0, &dir) < 0)  {
		printf("Error getting directory, you sure it exists?");
		return -1;
	}

	// Retrieve the data blocks the inode points to
	struct dirent* dir_inode_block = malloc(BLOCK_SIZE);
	for(int i = 0; i < 16; i++) {
		if(!dir.direct_ptr[i]) break;

		if(bio_read(dir.direct_ptr[i], dir_inode_block) < 0) {
			printf("Read error from disk from the direct pointer.");
			free(dir_inode_block);
			return -1;
		}

		// Copy each part.
		struct dirent* dir_ptr = dir_inode_block;
		for(int j = 0; j < (BLOCK_SIZE / sizeof(struct dirent)); j++, dir_ptr++) {
			if(dir_ptr->valid) {
				struct inode item_node;
				if(readi(dir_ptr->ino, &item_node) < 0) {
					printf("Error reading inode data inside readdir");
					return -1;
				}

				filler(buffer, dir_ptr->name, &item_node.vstat, 0);
			}
		}
		
	}
	free(dir_inode_block);
	return 0;
}


static int rufs_mkdir(const char *path, mode_t mode) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name

	// Step 2: Call get_node_by_path() to get inode of parent directory

	// Step 3: Call get_avail_ino() to get an available inode number

	// Step 4: Call dir_add() to add directory entry of target directory to parent directory

	// Step 5: Update inode for target directory

	// Step 6: Call writei() to write inode to disk
	dirname(path);

	return 0;
}

// Skip
static int rufs_rmdir(const char *path) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name

	// Step 2: Call get_node_by_path() to get inode of target directory

	// Step 3: Clear data block bitmap of target directory

	// Step 4: Clear inode bitmap and its data block

	// Step 5: Call get_node_by_path() to get inode of parent directory

	// Step 6: Call dir_remove() to remove directory entry of target directory in its parent directory

	return 0;
}

static int rufs_releasedir(const char *path, struct fuse_file_info *fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int rufs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target file name

	// Step 2: Call get_node_by_path() to get inode of parent directory

	// Step 3: Call get_avail_ino() to get an available inode number

	// Step 4: Call dir_add() to add directory entry of target file to parent directory

	// Step 5: Update inode for target file

	// Step 6: Call writei() to write inode to disk

	return 0;
}

static int rufs_open(const char *path, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path

	// Step 2: If not find, return -1

	return 0;
}

static int rufs_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {

	// Step 1: You could call get_node_by_path() to get inode from path

	// Step 2: Based on size and offset, read its data blocks from disk

	// Step 3: copy the correct amount of data from offset to buffer

	// Note: this function should return the amount of bytes you copied to buffer
	return 0;
}

static int rufs_write(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {
	// Step 1: You could call get_node_by_path() to get inode from path

	// Step 2: Based on size and offset, read its data blocks from disk

	// Step 3: Write the correct amount of data from offset to disk

	// Step 4: Update the inode info and write it to disk

	// Note: this function should return the amount of bytes you write to disk
	return size;
}

// Skip
static int rufs_unlink(const char *path) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target file name

	// Step 2: Call get_node_by_path() to get inode of target file

	// Step 3: Clear data block bitmap of target file

	// Step 4: Clear inode bitmap and its data block

	// Step 5: Call get_node_by_path() to get inode of parent directory

	// Step 6: Call dir_remove() to remove directory entry of target file in its parent directory

	return 0;
}

static int rufs_truncate(const char *path, off_t size) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int rufs_release(const char *path, struct fuse_file_info *fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
	return 0;
}

static int rufs_flush(const char * path, struct fuse_file_info * fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int rufs_utimens(const char *path, const struct timespec tv[2]) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}


static struct fuse_operations rufs_ope = {
	.init		= rufs_init,
	.destroy	= rufs_destroy,

	.getattr	= rufs_getattr,
	.readdir	= rufs_readdir,
	.opendir	= rufs_opendir,
	.releasedir	= rufs_releasedir,
	.mkdir		= rufs_mkdir,
	.rmdir		= rufs_rmdir,

	.create		= rufs_create,
	.open		= rufs_open,
	.read 		= rufs_read,
	.write		= rufs_write,
	.unlink		= rufs_unlink,

	.truncate   = rufs_truncate,
	.flush      = rufs_flush,
	.utimens    = rufs_utimens,
	.release	= rufs_release
};


int main(int argc, char *argv[]) {
	int fuse_stat;

	getcwd(diskfile_path, PATH_MAX);
	strcat(diskfile_path, "/DISKFILE");

	fuse_stat = fuse_main(argc, argv, &rufs_ope, NULL);

	return fuse_stat;
}

