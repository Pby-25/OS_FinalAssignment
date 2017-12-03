
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
#define NUM_INODES 100 // maximum number of inodes

/* macros */
#define FREE_BIT(_data, _which_bit) \
    _data = _data | (1 << _which_bit)

#define USE_BIT(_data, _which_bit) \
    _data = _data & ~(1 << _which_bit)


// Declare the global variables
// initialize pointers as null pointers to avoid undefined free()
superblock_t *sb_table = NULL;
inode_t *in_table = NULL;
file_descriptor *fd_table = NULL;
directory_entry *rootDir = NULL;

//initialize all bits to high
uint8_t free_bit_map[BITMAP_ROW_SIZE] = { [0 ... BITMAP_ROW_SIZE - 1] = UINT8_MAX };

// Helper function that initialize the superblock section
void init_super(){
	// initialize the superblock table
	free(sb_table);
	sb_table = malloc(sizeof(superblock_t));
	// assign the initial values
	sb_table->magic = MAGIC_NUM;
	sb_table->block_size = DEFAULT_BLOCK_SIZE;
	sb_table->fs_size = NUM_BLOCKS;
	sb_table->inode_table_len = NUM_INODES;
	sb_table->root_dir_inode = rootDir;
}

// Helper function that initialize the inode table
void init_int(){
	// initialize the inode table array
	free(in_table);
	in_table = malloc(sizeof(inode_t)*NUM_INODES);
	// assign the initial values
	for (int i=0; i<NUM_INODES; i++){
		in_table[i].mode = -1;
		in_table[i].link_cnt = -1;
		in_table[i].uid = -1;
		in_table[i].gid = -1;
		in_table[i].size = -1;
		// TODO
		// in_table[i].data_ptrs;
		// in_table[i].indirectPointer;
		
	}
}

// Helper function that initialize the file descriptor table
void init_fdt(){
	
}

void mksfs(int fresh) {
	if (fresh) {
		// initialize a fresh disk
		init_fresh_disk(DU_ZHUOCHENG_DISK, DEFAULT_BLOCK_SIZE, NUM_BLOCKS);

		// 
	} else {
		init_disk(DU_ZHUOCHENG_DISK, DEFAULT_BLOCK_SIZE, NUM_BLOCKS);
	}
}


int sfs_getnextfilename(char *fname){

}
int sfs_getfilesize(const char* path){

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

