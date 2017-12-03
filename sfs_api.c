
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
		fd_table[i].rwptr = -1;
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
		dir_pos++;
		if (rootDir[dir_pos].num != -1){
			strcpy(fname, rootDir[dir_pos].name);
			return 1;
		} else {
			// all the files have been returned
			break;
		}
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
		} else if (rootDir[current].num = -1) {
			break; // reached the end of directory entry table
		}
		current++;
	}
	printf("File does not exist\n");
	return 0;
}

int sfs_fopen(char *name){

}

int sfs_fclose(int fileID) {

}

int sfs_fread(int fileID, char *buf, int length) {
	
}

int sfs_fwrite(int fileID, const char *buf, int length) {

}

int sfs_fseek(int fileID, int loc) {

}

int sfs_remove(char *file) {


}
