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

bitmap_t inode_bmap;
bitmap_t data_bmap;
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

	for(int i = 0; i < MAX_INUM; i++) {
		if(get_bitmap(inode_bmap, i) == 0) {
			set_bitmap(inode_bmap, i);
			bio_write(superblk->i_bitmap_blk, inode_bmap);
			printf("\nWE ARE RETURNING FROM GET_AVAIL %d\n", i);
			return i;
		}
	}

	printf("\nWE ARE RETURNING FROM GET_AVAIL NEGATIVE 1\n");

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
			bio_write(superblk->d_bitmap_blk, data_bmap);
			return (i + superblk->d_start_blk);
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
	struct inode *block = malloc(BLOCK_SIZE);
    if (!block) {
        perror("Memory allocation failed in readi");
        return -ENOMEM;
    }

	if(bio_read(block_no, block) < 0) {
		printf("Read Error within readi");
		free(block);
		return -EIO;
	}

	// struct inode *ptr_to_inode = *block + (offset * sizeof(inode));
    // struct inode *ptr_to_inode = (struct inode *)((char *)block + offset * sizeof(struct inode));
	struct inode *ptr_to_inode = &block[offset];

	memcpy(inode, ptr_to_inode, sizeof(struct inode));

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
	struct inode *block = malloc(BLOCK_SIZE);
	if (!block) {
        perror("Memory allocation failed in writei");
        return -ENOMEM;
    }

	if(bio_read(block_no, block) < 0) {
		printf("Read Error within writei");
		free(block);
		return -EIO;
	}

	// struct inode *ptr_to_inode = *block + (offset * sizeof(inode));
    // struct inode *ptr_to_inode = (struct inode *)((char *)block + offset * sizeof(struct inode));
	struct inode *ptr_to_inode = &block[offset];

	// Copy the inode to the place where it belongs in the block
	memcpy(ptr_to_inode, inode, sizeof(struct inode));

	// Write the entire block back to the disk.
	if (bio_write(block_no, block) < 0) {
        printf("Write Error within writei\n");
        free(block);
        return -EIO;
    }

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
  	printf("Currently inside dir_find with fname: %s\n", fname);
	struct inode *dir_inode = malloc(sizeof(struct inode));

	if(readi(ino, dir_inode) < 0) {
		printf("Error finding inode inside dir_find\n");
		return -EIO;
	}

	// Get data block of current directory
	struct dirent *curr_dir_block = malloc(BLOCK_SIZE);

	// char* dup = strdup(fname);
	// char* token;
	// token = strsep(&dup, "/");
	char* token = fname;
	if(fname[0] == '/') {
		token += 1;

	}

	printf("This is the current token: %s\n", token);

	// read through direct pointers?
	int end = dir_inode->size / BLOCK_SIZE;
	for(int ptr = 0; ptr < end; ptr++) {
		
		// Dont exist
		if(!dir_inode->direct_ptr[ptr]) {
			printf("direct pointer %i does not exist!\n", ptr);
			free(dir_inode);
			// free(dup);
			return -ENOENT;
		}

		if(bio_read(dir_inode->direct_ptr[ptr], curr_dir_block) < 0) {
			printf("Error reading from disk within dir_find\n");
			free(dir_inode);
			// free(dup);
			return -EIO;
		}

		// Now I should have the data block. Read thru the whole thing for dir entries
		struct dirent *dir_ptr = curr_dir_block;
		for(int i = 0; i < (BLOCK_SIZE / sizeof(struct dirent)); i++, dir_ptr++) {
			printf("Currently director is called: %s\n", dir_ptr->name);
			if(!dir_ptr->valid) continue;
			else if(strcmp(dir_ptr->name, token) == 0) {
				memcpy(dirent,dir_ptr, sizeof(struct dirent));
				free(curr_dir_block);
				free(dir_inode);
				// free(dup);

				return 0;
			}
		}

	}

	free(curr_dir_block);
	free(dir_inode);
	// free(dup);
	return -ENOENT;
}

// initializes block full of dirent structs and sets them all to invalid
int dirent_block_init(int data_block){

	struct dirent *dirent_ptr = malloc(BLOCK_SIZE);
	if(bio_read(data_block, dirent_ptr) < 0) {
		printf("Error reading from disk within dirent_block_init");
		free(dirent_ptr);
		return -ENOENT;
	}

	size_t num_entries = BLOCK_SIZE / sizeof(struct dirent);

	for (size_t i = 0; i < num_entries; i++) {
		dirent_ptr[i].ino = 0;
        dirent_ptr[i].valid = 0;  // invalid
		memset(dirent_ptr[i].name, 0, sizeof(dirent_ptr[i].name));
		dirent_ptr[i].len = 0;
    }

	if(bio_write(data_block, dirent_ptr) < 0) {
		printf("error writing to file from dir_add");
		free(dirent_ptr);
		return -EIO;
	}

	free(dirent_ptr);
	return 0;
}

int dir_add(struct inode dir_inode, uint16_t f_ino, const char *fname, size_t name_len) {

	// Step 1: Read dir_inode's data block and check each directory entry of dir_inode
	
	// Step 2: Check if fname (directory name) is already used in other entries

	// Step 3: Add directory entry in dir_inode's data block and write to disk

	// Allocate a new data block for this directory if it does not exist

	// Update directory inode

	// Write directory entry

	printf("START OF DIR ADD \ndir_inode: %d \nfile ino: %d\n file name: %s\n", dir_inode.ino, f_ino, fname);

	// Dont exist
	// if(!dir_inode.direct_ptr[0]) {
	// 	// Create a data block entry.
	// 	printf("Currently no pointer to data\n");
	// 	int new_data_block = get_avail_blkno();
	// 	dir_inode.direct_ptr[0] = new_data_block;
	// 	writei(dir_inode.ino, &dir_inode);

	// 	// Add the entry.
	// 	struct dirent* new_dir = malloc(BLOCK_SIZE);

	// 	new_dir[0] = (struct dirent) {
	// 		.ino = f_ino,
	// 		.valid = 1,
	// 		.len = (uint16_t) strlen(fname)
	// 	};
	// 	strcpy(new_dir[0].name, fname);

	// 	printf("Written new directory (%s) to inode %u\n", fname, f_ino);
		
	// 	if(bio_write(new_data_block, new_dir) < 0) {
	// 		printf("error writing to file from dir_add");
	// 		free(new_dir);
	// 		return -EIO;
	// 	}

	// 	dir_inode.link += 1;
	// 	dir_inode.vstat.st_nlink += 1;
	// 	free(new_dir);
	// 	return 0;
	// }

	// There are data blocks alr...
	// First try and find it...
	struct dirent searching_dir;
	if(dir_find(dir_inode.ino, fname, name_len, &searching_dir) == 1) {
		printf("File name already exists\n");
		return -EEXIST;
	}

	struct dirent *curr_dir_block = malloc(BLOCK_SIZE);

	// read through direct pointers
	int end = dir_inode.size / BLOCK_SIZE;
	for(int ptr = 0; ptr < end; ptr++) {
		
		if(bio_read(dir_inode.direct_ptr[ptr], curr_dir_block) < 0) {
			printf("Error reading from disk within dir_add");
			free(curr_dir_block);
			return -ENOENT;
		}

		// Now I should have the data block. Read thru the whole thing for dir entries
		struct dirent *dir_ptr = curr_dir_block;
		for(int i = 0; i < (BLOCK_SIZE / sizeof(struct dirent)); i++, dir_ptr++) {
			// Find a place i can insert. REMINDER that because i am searching for a invalid entry to insert into, i need to invalidate entries
			if(dir_ptr->valid) continue;
			else {
				// Add the entry.
				printf("Found an entry at block offset %d\n", i);

				*dir_ptr = (struct dirent) {
					.ino = f_ino,
					.valid = 1,
					.len = (uint16_t) strlen(fname)
				};

				strcpy(dir_ptr->name, fname);

				printf("Wrote directory entry %s to offset %d\n", dir_ptr->name, i);

				// More stats I need to add to this for sure
				dir_inode.size += sizeof(struct dirent);

				if(writei(dir_inode.ino, &dir_inode) < 0) {
					printf("error writing to file from dir_add");
					free(curr_dir_block);

					return -ENOENT;
				}

				// Write the new block with the new directory to the disk
				if(bio_write(dir_inode.direct_ptr[ptr], curr_dir_block) < 0) {
					printf("Write error inside dir_add");
					free(curr_dir_block);
					return -EIO;
				}

				dir_inode.link += 1;
				dir_inode.vstat.st_nlink += 1;
				free(curr_dir_block);
				return 0;
			}
		}

	}

	free(curr_dir_block);
	return -EIO;
}

// Skip
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
	// char *path_cpy = strdup(path);

	// char* token = strtok(path_cpy, '/');

	char *token, *str, *str_cpy;

	str_cpy = str = strdup(path);

	printf("The Path we are checking is: %s\n", path);

	if (!strcmp(path, "/")) {
		return readi(0, inode);
	}

	struct dirent* temp_dir = malloc(sizeof(struct dirent));
	temp_dir->ino = 0; // Root
	while ((token = strsep(&str, "/")) != NULL) {
		if(token[0] != '\0') {
			printf("Inside get_node_by_path, token is: %s\n", token);
			int ret = dir_find(temp_dir->ino, token, strlen(token), temp_dir);
			if(ret < 0) {
				free(temp_dir);
				free(str_cpy);
				return ret;
			}
		}
	}

	printf("Currently the directory entry is: %s\n", temp_dir->name);

	// It should have found it by now
	if(temp_dir->valid) {
		if(readi(temp_dir->ino, inode) < 0) {
			printf("error reading inode block inside get_node_by_path");
			free(temp_dir);
			free(str_cpy);
			return -EIO;
		}
		// Found it
	}

	free(temp_dir);
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
	// struct dirent root_dir;


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
	if(root_ino_num != 0) {
		printf("Error setting root inode, no available inode number");
		return -1;
	}

	// For directory data block
	int root_blk_num = get_avail_blkno();
	if(root_blk_num != superblk->d_start_blk) {
		printf("Error setting root inode, no available block number");
		return -1;
	}

	// Set the bmap for the root directory.
	root_ino = (struct inode) {
		.ino = root_ino_num,
		.valid = 1,
		.size = BLOCK_SIZE,
		.type = 1,
		.link = 2,
		.direct_ptr[0] = superblk->d_start_blk,
		.vstat = (struct stat){
			.st_mode  = S_IFDIR | 0755,
			.st_nlink = 2,
			.st_mtime = time(NULL),
			.st_atime = time(NULL),
			.st_uid = getuid(),
			.st_gid = getgid()
		}
	};

	if (dirent_block_init(superblk->d_start_blk) < 0) {
		printf("Error setting root dirent block\n");
		return -1;
	}

	// Write the root inode to the inode block
	bio_write(superblk->i_start_blk, &root_ino);


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
	struct inode* path_inode = malloc(sizeof(struct inode));
	printf("Checking if the node exists!\n");
	if(get_node_by_path(path, 0, path_inode) < 0) {
		// Not found
		free(path_inode);
		return -ENOENT;
	}

	printf("Node exists with inode number: %u\n", path_inode->ino);


	*stbuf = path_inode->vstat;
	fprintf(stderr, "find_inode_by_path: path=%s mode=%o\n", path, path_inode->vstat.st_mode);
	free(path_inode);
	return 0;
}

static int rufs_opendir(const char *path, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path

	// Step 2: If not find, return -1

	struct inode* dir = malloc(sizeof(struct inode));

	int ret = get_node_by_path(path, 0, dir);

	free(dir);
	return ret;
}

static int rufs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path

	// Step 2: Read directory entries from its data blocks, and copy them to filler

	struct inode* dir = malloc(sizeof(struct inode));

	printf("Reading directory entries\n");

	if(get_node_by_path(path, 0, dir) < 0)  {
		printf("Error getting directory, you sure it exists?\n");
		free(dir);
		return -ENOENT;
	}

	// Retrieve the data blocks the inode points to
	struct dirent* dir_inode_block = malloc(BLOCK_SIZE);
	int end = dir->size / BLOCK_SIZE;
	for(int i = 0; i < end; i++) {
		if(!dir->direct_ptr[i]) break;

		if(bio_read(dir->direct_ptr[i], dir_inode_block) < 0) {
			printf("Read error from disk from the direct pointer.");
			free(dir);
			free(dir_inode_block);
			return -ENOENT;
		}

		printf("Directory entry is at block num: %u\n", dir->direct_ptr[i]);

		// Copy each part.
		struct dirent* dir_ptr = dir_inode_block;
		for(int j = 0; j < (BLOCK_SIZE / sizeof(struct dirent)); j++, dir_ptr++) {
			if(dir_ptr->valid) {
				printf("node filename: %s\n", dir);
				struct inode item_node;
				if(readi(dir_ptr->ino, &item_node) < 0) {
					printf("Error reading inode data inside readdir");
					free(dir);
					return -ENOENT;
				}
				printf("item: %s, type: %d\n", dir_ptr->name, item_node.type); 

				filler(buffer, dir_ptr->name, &item_node.vstat, 0);
			}
		}
		
	}

	free(dir);
	free(dir_inode_block);
	return 0;
}


static int rufs_mkdir(const char *path, mode_t mode) {
	printf("MKDIR called\n");

	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name

	// Step 2: Call get_node_by_path() to get inode of parent directory

	// Step 3: Call get_avail_ino() to get an available inode number

	// Step 4: Call dir_add() to add directory entry of target directory to parent directory

	// Step 5: Update inode for target directory

	// Step 6: Call writei() to write inode to disk
	char* stripped = strdup(path);
	char* basen = strdup(path);
	char* dir = dirname(stripped);
	char* basenm = basename(basen);

	printf("Dirname: %s, Basename: %s\n", dir, basenm);

	struct inode* par_inode = malloc(sizeof(struct inode));

	int erro_code = get_node_by_path(dir, 0, par_inode);
	if(erro_code < 0) {
		printf("Error getting parent inode");
		free(par_inode);
		free(dir);
		free(basen);
		return erro_code;
	}

	printf("This is the parent node: #: %u\n", par_inode->ino);

	// struct dirent temp_dir;
	// dir_find(par_inode.ino, basenm, strlen(basenm), &temp_dir);

	// if(temp_dir.valid) {
	// 	return -1;
	// }


	int new_dir_ino_num = get_avail_ino();

	printf("CALLING DIR_ADD for mkdir\nparent path: %s\nparent inode: %d\nf_ino: %d\n", dir, par_inode->ino, new_dir_ino_num);

	int error_no;
	if((error_no = dir_add(*par_inode, new_dir_ino_num, basenm, strlen(basenm))) < 0) {
		printf("error adding directory entry");
		free(par_inode);
		free(dir);
		free(basen);
		return error_no;
	}

	struct inode dir_inode;
	dir_inode = (struct inode) {
		.ino = new_dir_ino_num,
		.valid = 1,
		.type = 1,
		.size = BLOCK_SIZE,
		.link = 2,
		.direct_ptr[0] = get_avail_blkno(),	// Reserve the space
		.vstat = (struct stat) {
			.st_mode  = S_IFDIR | 0755,
			.st_nlink = 2,
			.st_mtime = time(NULL),
			.st_atime = time(NULL),
			.st_uid = getuid(),
			.st_gid = getgid()
		}
	};

	if (dirent_block_init(dir_inode.direct_ptr[0]) < 0) {
		free(dir);
		free(basen);
		free(par_inode);
		return error_no;
	}

	printf("New Directory Inode made with # %u with direct pointer at: %u\n", dir_inode.ino, dir_inode.direct_ptr[0]);

	writei(new_dir_ino_num, &dir_inode);
	// Accounts for "." and ".."
	// bio_write(dir_inode.direct_ptr[0], ...);
	// bio_write(dir_inode.direct_ptr[1], ...);

	free(par_inode);
	free(dir);
	free(basen);
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

	char* stripped = strdup(path);
	char* basen = strdup(path);
	char* dir = dirname(stripped);
	char* basenm = basename(basen);

	printf("Dirname: %s, Basename: %s\n", dir, basenm);

	struct inode* par_inode = malloc(sizeof(struct inode));

	int erro_code = get_node_by_path(dir, 0, par_inode);
	if(erro_code < 0) {
		printf("Error getting parent inode");
		free(par_inode);
		free(dir);
		free(basen);
		return erro_code;
	}


	printf("This is the parent node: #: %u\n", par_inode->ino);

	int new_dir_ino_num = get_avail_ino();

	printf("CALLING DIR_ADD for file create\nparent path: %s\nparent inode: %d\nf_ino: %d\n", dir, par_inode->ino, new_dir_ino_num);

	int error_no;
	if((error_no = dir_add(*par_inode, new_dir_ino_num, basenm, strlen(basenm))) < 0) {
		printf("error adding directory entry");
		free(par_inode);
		free(dir);
		free(basen);
		return error_no;
	}

	struct inode dir_inode;
	dir_inode = (struct inode) {
		.ino = new_dir_ino_num,
		.valid = 1,
		.type = 2,
		.size = BLOCK_SIZE,
		.link = 1,
		.direct_ptr[0] = get_avail_blkno(),	// Reserve the space
		.vstat = (struct stat) {
			.st_mode  = S_IFREG | 0644,
			.st_nlink = 1,
			.st_mtime = time(NULL),
			.st_atime = time(NULL),
			.st_uid = getuid(),
			.st_gid = getgid(),
			.st_size = BLOCK_SIZE,
		}
	};

	printf("New Directory Inode made with # %u with direct pointer at: %u\n", dir_inode.ino, dir_inode.direct_ptr[0]);

	writei(new_dir_ino_num, &dir_inode);

	free(par_inode);
	free(dir);
	free(basen);
	return 0;
}

static int rufs_open(const char *path, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path

	// Step 2: If not find, return -1

	struct inode* file_inode = malloc(sizeof(struct inode));

	int i = get_node_by_path(path, 0, file_inode);
	free(file_inode);
	return (i < 0) ? -1 : 0;
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

