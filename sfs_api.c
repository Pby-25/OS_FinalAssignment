
#include "sfs_api.h"
#include "bitmap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fuse.h>
#include <strings.h>
#include "disk_emu.h"
#define DU_ZHUOCHENG_DISK "sfs_disk.disk"
#define NUM_BLOCKS 1024  //maximum number of data blocks on the disk.
#define BITMAP_ROW_SIZE (NUM_BLOCKS/8) // this essentially mimcs the number of rows we have in the bitmap. we will have 128 rows. 

#define MAGIC_NUM 0xACBD0005 // format code for the file system
#define DEFAULT_BLOCK_SIZE 1024
#define NUM_INODES 100 // maximum number of inodes (specification given in the assignment)
#define ROOT_INODE 0 // index of the root directory

/* macros */
#define FREE_BIT(_data, _which_bit) \
    _data = _data | (1 << _which_bit)

#define USE_BIT(_data, _which_bit) \
    _data = _data & ~(1 << _which_bit)


/*------------------------------------------------------------------*/
/*						   GLOBAL VARIABLES				            */
/*------------------------------------------------------------------*/
// initialize pointers as null pointers to avoid undefined free()
superblock_t *sb_table = NULL;
inode_t *in_table = NULL;
file_descriptor *fd_table = NULL;
directory_entry *rootDir = NULL;

// initialize all bits to high
uint8_t free_bit_map[BITMAP_ROW_SIZE] = { [0 ... BITMAP_ROW_SIZE - 1] = UINT8_MAX };

// initialize a variable to record current position in directory
int dir_pos = 0;

/*------------------------------------------------------------------*/


// Helper function that initialize the superblock section
void init_super(){
	// initialize the superblock table
	// free(sb_table);
	sb_table = malloc(sizeof(superblock_t));
	// assign the initial values
	sb_table->magic = MAGIC_NUM;
	sb_table->block_size = DEFAULT_BLOCK_SIZE;
	sb_table->fs_size = NUM_BLOCKS;
	sb_table->inode_table_len = NUM_INODES;
	sb_table->root_dir_inode = ROOT_INODE;
	
}

// Helper function that initialize the inode table
void init_int(){
	// initialize the inode table array
	// free(in_table);
	in_table = malloc(sizeof(inode_t)*NUM_INODES);
	// assign the initial values
	for (int i=0; i<NUM_INODES; i++){
		in_table[i].mode = -1;
		in_table[i].link_cnt = -1;
		in_table[i].uid = -1;
		in_table[i].gid = -1;
		in_table[i].size = -1;
		for (int j=0; j<12; j++){
			in_table[i].data_ptrs[j] = -1;
		}	
		in_table[i].indirectPointer = -1;
		
	}
}

// Helper function that initialize the file descriptor table
void init_fdt(){
	// free(fd_table);
	fd_table = malloc(sizeof(file_descriptor*(NUM_INODES)));
	// assign the initial values
	for (int i=0; i<NUM_INODES; i++){
		fd_table[i].inodeIndex = -1;
		fd_table[i].inode = NULL;
		fd_table[i].rwptr = 0;
	}
}

// Helper function that initialize the root directory table
void init_rdt(){
	rootDir = malloc(sizeof(directory_entry)*NUM_INODES);
	for (int i=0; i<NUM_INODES; i++){
		rootDir[i].num = -1;
	}
}

// Helper function that calculate the number of block needed
unsigned int block_req(unsigned int n){
	return (n + DEFAULT_BLOCK_SIZE-1)/DEFAULT_BLOCK_SIZE; // ceil divison by 1024, ignore the unlikely event of overflowing
}

void mksfs(int fresh) {
	unsigned int super_size = block_req(sizeof(superblock_t));
	unsigned int inodes_size = block_req(sizeof(inode_t)*NUM_INODES);
	unsigned int dir_size = block_req(sizeof(directory_entry)*NUM_INODES);

	if (fresh) {
		// initialize a fresh disk
		init_fresh_disk(DU_ZHUOCHENG_DISK, DEFAULT_BLOCK_SIZE, NUM_BLOCKS); //TODO

		// initialzie the global variabes
		init_super();
		init_int();
		init_fdt();
		init_rdt();

		init_bitmap();

		// initialize the bitma
		int index = 0;
		// superblock
		for(index; index < super_size; index++){
			force_set_index(index);
		}
		// inode table
		for(index; index < inodes_size + super_size; index++){
			force_set_index(index);
		}
		// directory entry table
		for(index; index < dir_size + inodes_size + super_size; index++){
			force_set_index(index);
		}
		// bitmap itself
		force_set_index(NUM_BLOCKS-1);		
		
		// setup the root directory inode
		in_table[ROOT_INODE].size =  dir_size;
		for (int i=0; i<block_req(dir_size); i++){ // the root directory table occupies less than 12 blocks
			in_table[ROOT_INODE].data_ptrs[i] = i + inodes_size + super_size;
		}

		// format the emulated disk
		write_blocks(0, super_size, sb_table);
		write_blocks(super_size, inodes_size, in_table);
		write_blocks(super_size + inodes_size, dir_size, rootDir);
		write_blocks(NUM_BLOCKS-1, 1, free_bit_map);

	} else {
		init_disk(DU_ZHUOCHENG_DISK, DEFAULT_BLOCK_SIZE, NUM_BLOCKS);
		// Read data into appropriate global variables
		read_blocks(0, super_size, sb_table);// TODO
		read_blocks(super_size, inodes_size, in_table);
		read_blocks(super_size + inodes_size, dir_size, rootDir);
		read_blocks(NUM_BLOCKS-1, 1, free_bit_map);
		// initialize the file directory in memory
		init_fdt();

	}
}


int sfs_getnextfilename(char *fname){
	while (dir_pos < NUM_INODES){
		if (rootDir[dir_pos].num != -1){
			strncpy(fname, rootDir[dir_pos].name, MAX_FILE_NAME);
			return 1;
		}
		dir_pos++;
	}
	printf("Reached end of directory\n");
	dir_pos = 0;
	return 0;
}

int sfs_getfilesize(const char* path){
	// Since there's only a root directory in this assignment, file path is equivalent to file length 
	// According to provided test cases, there's slash symbol before filename
	int current = 0;
	while (current < NUM_INODES){
		if (strcmp(path, rootDir[current].name)==0){
			return in_table[rootDir[current].num].size;
		} 
		current++;
	}
	printf("File does not exist\n");
	return -1;
}

int sfs_fopen(char *name){
	int current = 0;
	int i = 0;
	while (current < NUM_INODES){
		// If the file exists
		if (strcmp(name, rootDir[current].name)==0){
			while(i<NUM_INODES){
				if(fd_table[i].inodeIndex == -1){
					// load the corresponding inode into file descriptor
					fd_table[i].inodeIndex = rootDir[current].num;
					fd_table[i].inode = &in_table[rootDir[current].num];
					fd_table[i].rwptr = in_table[rootDir[current].num].size;
					return i;
				}
				i++;
			}
			printf("Reached maximum file descriptor allowance\n");
			return -1;
		}
		current++;
	}
	// if the file does not exist, make a new one
	current = 0;
	while (current < NUM_INODES){
		if (rootDir[current].num == -1) break;
		current++;
		}
	i =0;
	while(i<NUM_INODES){
		if(fd_table[i].inodeIndex == -1){
			int j = 0;
			while (j<NUM_INODES){
				if(in_table[j].size == -1){
					// setup the inode table
					in_table[j].size = 0;
					// setup the directory entry
					rootDir[current].num = j;
					strncpy(rootDir[current].name, name, MAX_FILE_NAME);
					// setup the file descriptor
					fd_table[i].inodeInde = j;
					fd_table[i].inode = &in_table[j];
					return i;
				}
				j++;
			}
			printf("Reached maximum inode allowance\n");
			return -1;
		}
		i++;
	}
	printf("Reached maximum file descriptor allowance\n");
	return -1;

}

int sfs_fclose(int fileID) {
	fd_table[fildID].inodeIndex = -1;
	fd_table[fildID].inode = NULL;
	fd_table[fildID].rwptr = 0;
	return 0;
}

int sfs_fread(int fileID, char *buf, int length) {
	char *tmp = malloc(DEFAULT_BLOCK_SIZE);
	int shift = fd_table[fildID].rwptr / DEFAULT_BLOCK_SIZE;
	int rem = fd_table[fildID].rwptr % DEFAULT_BLOCK_SIZE;

	while(length > 0){
		
		// Read from disk to a temporary buffer
		if(shift < 12){ 
			read_blocks(fd_table[fileID].inode->data_ptrs[shift], 1, tmp);
		} else { // the data pointer reside in indirect pointer part
			//TODO
		}

		// Copy from temporary buffer to destination buffer
		if (length <= DEFAULT_BLOCK_SIZE - rem){ // reached the last part of data requested
			memcpy(buf, tmp + rem, length);
		} else{
			memcpy(buf, tmp + rem, DEFAULT_BLOCK_SIZE - rem);
		}
		shift++;
		length -= DEFAULT_BLOCK_SIZE;
		buf += DEFAULT_BLOCK_SIZE;
		if (rem > 0) rem = 0; // if this is the first part of data requested		
	}
	free(tmp);
	return 0;
}

int sfs_fwrite(int fileID, const char *buf, int length) {
	char *tmp = malloc(DEFAULT_BLOCK_SIZE);
	int shift = fd_table[fildID].rwptr / DEFAULT_BLOCK_SIZE;
	int rem = fd_table[fildID].rwptr % DEFAULT_BLOCK_SIZE;
	
	while (length > 0){
	
		// copy from origin buffer to temporary buffer
		if (rem > 0){ // first section to be written
			read_blocks(fd_table[fileID].inode->data_ptrs[shift], 1, tmp);
		}	
		if(length + rem <= DEFAULT_BLOCK_SIZE){ // if this is the last part of data
			memcpy(tmp + rem, buf, length);
		} else{
			memcpy(tmp + rem, buf, DEFAULT_BLOCK_SIZE);
		}

		// write to disk
		if (shift < 12){
			// allocate space if needed
			if (fd_table[fileID].inode->data_ptrs[shift] == -1){
				fd_table[fileID].inode->data_ptrs[shift] = get_index();
			}
			write_blocks(fd_table[fileID].inode->data_ptrs[shift], 1, tmp);

		} else { // if indirect pointer need to be used
			//TODO
		}
		shift++;
		length -= DEFAULT_BLOCK_SIZE;
		buf += DEFAULT_BLOCK_SIZE;
		if (rem > 0) rem = 0;
	}
	// update the possible changes to bitmap
	write_blocks(NUM_BLOCKS-1, 1, free_bit_map);
	free(tmp);
	return 0;
}

int sfs_fseek(int fileID, int loc) {
	fd_table[fileID].rwptr = loc;
}

int sfs_remove(char *file) {
	
}
