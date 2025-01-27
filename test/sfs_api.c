
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

#define MAGIC_NUM 0xACBD0005 // format code for the file system
#define DEFAULT_BLOCK_SIZE 1024
#define NUM_INODES 100 // maximum number of inodes (specification given in the assignment)
#define ROOT_INODE 0 // index of the root directory

#define BLOCK_REQ(n) ((n+DEFAULT_BLOCK_SIZE-1)/DEFAULT_BLOCK_SIZE)

#define SUPER_BLOCK_N BLOCK_REQ(sizeof(superblock_t))
#define INODES_BLOCK_N BLOCK_REQ(sizeof(inode_t)*NUM_INODES)
#define DIR_BLOCK_N BLOCK_REQ(sizeof(directory_entry)*NUM_INODES)

/*------------------------------------------------------------------*/
/*                           GLOBAL VARIABLES                       */
/*------------------------------------------------------------------*/
superblock_t sb_table;
inode_t in_table[NUM_INODES];
file_descriptor fd_table[NUM_INODES];
directory_entry rootDir[NUM_INODES];

// initialize a variable to record current position in directory
int dir_pos = -1;

/*------------------------------------------------------------------*/


// Helper function that initialize the superblock section
void init_super(){
    // assign the initial values
    sb_table.magic = MAGIC_NUM;
    sb_table.block_size = DEFAULT_BLOCK_SIZE;
    sb_table.fs_size = NUM_BLOCKS;
    sb_table.inode_table_len = NUM_INODES;
    sb_table.root_dir_inode = ROOT_INODE;

}

// Helper function that initialize the inode table
void init_int(){
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
    // assign the initial values
    for (int i=0; i<NUM_INODES; i++){
        fd_table[i].inodeIndex = -1;
        fd_table[i].inode = NULL;
        fd_table[i].rwptr = 0;
    }
}

// Helper function that initialize the root directory table
void init_rdt(){
    for (int i=0; i<NUM_INODES; i++){
        rootDir[i].num = -1;
        rootDir[i].name[0] = '\0';
    }
}


void mksfs(int fresh) {

    if (fresh) { // initialize a fresh disk
        init_fresh_disk(DU_ZHUOCHENG_DISK, DEFAULT_BLOCK_SIZE, NUM_BLOCKS);

        // initialzie the global variabes
        init_super();
        init_int();
        init_fdt();
        init_rdt();

        // all bits in bitmap is initialized to HIGH in bitmap.c

        // initialize the bitmap for the superblock, inodes, and directories
        for(int index = 0; index < DIR_BLOCK_N + INODES_BLOCK_N + SUPER_BLOCK_N; index++){
            force_set_index(index);
        }
        // bitmap itself
        force_set_index(NUM_BLOCKS-1);

        // setup the root directory inode
        in_table[ROOT_INODE].size =  sizeof(directory_entry)*NUM_INODES;
        for (int i=0; i<DIR_BLOCK_N; i++){ // it is known that the root directory table occupies less than 12 blocks
            in_table[ROOT_INODE].data_ptrs[i] = i + INODES_BLOCK_N + SUPER_BLOCK_N;
        }

        // format the emulated disk
        write_blocks(0, SUPER_BLOCK_N, &sb_table);
        write_blocks(SUPER_BLOCK_N, INODES_BLOCK_N, in_table);
        write_blocks(SUPER_BLOCK_N + INODES_BLOCK_N, DIR_BLOCK_N, rootDir);
        write_blocks(NUM_BLOCKS-1, 1, free_bit_map);

    } else { // load an existing disk
        init_disk(DU_ZHUOCHENG_DISK, DEFAULT_BLOCK_SIZE, NUM_BLOCKS);
        // Read data into appropriate global variables
        read_blocks(0, SUPER_BLOCK_N, &sb_table);
        read_blocks(SUPER_BLOCK_N, INODES_BLOCK_N, in_table);
        read_blocks(SUPER_BLOCK_N + INODES_BLOCK_N, DIR_BLOCK_N, rootDir);
        read_blocks(NUM_BLOCKS-1, 1, free_bit_map);
        // initialize the file directory in memory
        init_fdt();

    }
}


int sfs_getnextfilename(char *fname){
    while (dir_pos++ < NUM_INODES){
        if (rootDir[dir_pos].num != -1){ // if the position is occupied
            strncpy(fname, rootDir[dir_pos].name, MAX_FILE_NAME);
            return 1;
        }
    }
    printf("Reached end of directory\n");
    // refresh the global indicator
    dir_pos = -1;
    return 0;
}

int sfs_getfilesize(const char* path){
    // Since there's only a root directory in this assignment, file path is equivalent to file name
    // According to provided test cases, there's no slash symbol before filename
    int current = 0;
    while (current < NUM_INODES){ // avoided using sfs_getnextfilename() to keep the global variable intact
        if (strncmp(path, rootDir[current].name, MAX_FILE_NAME)==0){
            return in_table[rootDir[current].num].size;
        }
        current++;
    }
    printf("Attempted get size info about a file that does not exist\n");
    return -1;
}

int sfs_fopen(char *name){

    int current = 0;
    int unused_fd = 0;
    while (current < NUM_INODES){
        // If the file exists
        if (strncmp(name, rootDir[current].name, MAX_FILE_NAME)==0){
            // Make sure the file is not already opened
            int i = 0;
            while(i < NUM_INODES){
                if (fd_table[i].inodeIndex == rootDir[current].num){
                    return i;
                }
                i++;
            }
            while (unused_fd < NUM_INODES ){
                if(fd_table[unused_fd].inodeIndex == -1){
                    // load the corresponding inode into file descriptor
                    fd_table[unused_fd].inodeIndex = rootDir[current].num;
                    fd_table[unused_fd].inode = &in_table[rootDir[current].num];
                    fd_table[unused_fd].rwptr = in_table[rootDir[current].num].size;
                    return unused_fd;
                }
                unused_fd++;
            }

            printf("Reached maximum file descriptor allowance\n");
            return -1;
        }
        current++;
    }

    // if the file does not exist, make a new one
    // First let's make sure the name is acceptable
    int eligible = 0;
    char *tmp = name;
    int ext_p = MAX_FILE_NAME_SANS_EXT; // period after which the extension is found
    int ext_exist = 0; // true if extension name isn't null
    do{
        // Stop when reached the end of name, eligible unless it's an empty name
        if(*(tmp + eligible)=='\0'){
        break;
        // ineligible if the filename is too long
        } else if (eligible >= MAX_FILE_NAME){
        eligible = 0;
        break;
        }
        // record the last period to calculate extension length later
        if (*(tmp + eligible)=='.'){
        ext_p = eligible;
        ext_exist = 1;
        }
        eligible++;
    } while (eligible);

    // filename DNE or longer than 20 || extension longer than 3 || name w/o ext. longer than 16
    if (!eligible || eligible - ext_p > MAX_EXTENSION_NAME + 1 || ext_p > MAX_FILE_NAME_SANS_EXT
        || (!ext_exist && eligible > MAX_FILE_NAME_SANS_EXT)){
        printf("ineligible name for new file\n");
        return -1;
    }

    current = 0;
    while (current < NUM_INODES){ // find an empty spot in root directory
        if (rootDir[current].num == -1) break;
        current++;
        }
    unused_fd =0;
    while(unused_fd<NUM_INODES){
        if(fd_table[unused_fd].inodeIndex == -1){ // find an empty spot in file descriptor
            int unused_in = 0;
            while (unused_in<NUM_INODES){
                if(in_table[unused_in].size == -1){ // find an empty spot in inode table
                    // setup the inode table
                    in_table[unused_in].size = 0;
                    // setup the directory entry
                    rootDir[current].num = unused_in;
                    strncpy(rootDir[current].name, name, MAX_FILE_NAME);
                    // setup the file descriptor
                    fd_table[unused_fd].inodeIndex = unused_in;
                    fd_table[unused_fd].inode = &in_table[unused_in];
                    return unused_fd;
                }
                unused_in++;
            }
            printf("Reached maximum inode allowance\n");
            return -1;
        }
        unused_fd++;
    }
    printf("Reached maximum file descriptor allowance\n");
    return -1;
}

int sfs_fclose(int fileID){
    // Check if file is alrady closed
    if(fd_table[fileID].inodeIndex == -1){
        printf("Attempted to close a file that does not exist\n");
        return -1;
    }
    fd_table[fileID].inodeIndex = -1;
    fd_table[fileID].inode = NULL;
    fd_table[fileID].rwptr = 0;
    return 0;
}

int sfs_fread(int fileID, char *buf, int length) {
    if (fd_table[fileID].inodeIndex == -1){
        printf("file not found in file descriptor (fread)\n");
        return -1;
    }
    void *tmp = malloc(DEFAULT_BLOCK_SIZE);
    int shift = fd_table[fileID].rwptr / DEFAULT_BLOCK_SIZE;
    int rem = fd_table[fileID].rwptr % DEFAULT_BLOCK_SIZE;
    int read_count = 0;
    uint16_t *ind_ptr = NULL;

    // Avoid reading garbage values
    if (fd_table[fileID].rwptr + length > fd_table[fileID].inode->size){
    length = fd_table[fileID].inode->size - fd_table[fileID].rwptr;
    }

    // check if an indirect pointer is needed after removing garbage values
    int iptr_req = BLOCK_REQ(fd_table[fileID].rwptr + length) > 12;
    if (iptr_req){
        ind_ptr = malloc(DEFAULT_BLOCK_SIZE);
        read_blocks(fd_table[fileID].inode->indirectPointer, 1, ind_ptr);
    }

    while(length > 0){

        // Read from disk to a temporary buffer
        if(shift < 12){
            read_blocks(fd_table[fileID].inode->data_ptrs[shift], 1, tmp);
        } else { // the data pointer reside in indirect pointer part
            read_blocks(ind_ptr[shift-12], 1, tmp);
        }

        // Copy from temporary buffer to destination buffer
        if (length <= DEFAULT_BLOCK_SIZE - rem){ // reached the last part of data requested
            memcpy(buf, tmp + rem, length);
            read_count += length;
        } else{
            memcpy(buf, tmp + rem, DEFAULT_BLOCK_SIZE - rem);
            read_count += (DEFAULT_BLOCK_SIZE - rem);
        }
        shift++;
        length -= (DEFAULT_BLOCK_SIZE - rem);
        buf += (DEFAULT_BLOCK_SIZE - rem);
        if (rem > 0) rem = 0; // if this is the first part of data requested
    }
    free(tmp);
    if (iptr_req){
        free(ind_ptr);
    }
    // update the rw pointer
    fd_table[fileID].rwptr += read_count;
    return read_count;
}


int sfs_fwrite(int fileID, const char *buf, int length) {
    if (fd_table[fileID].inodeIndex == -1){
        printf("file not found in file descriptor (fwrite)\n");
        return -1;
    }
    int write_count = 0;
    void *tmp = malloc(DEFAULT_BLOCK_SIZE);
    int shift = fd_table[fileID].rwptr / DEFAULT_BLOCK_SIZE;
    int rem = fd_table[fileID].rwptr % DEFAULT_BLOCK_SIZE;
    uint16_t *ind_ptr = NULL;

    // check if it is necessary to access the indirect pointer section
    int iptr_req = BLOCK_REQ(fd_table[fileID].rwptr + length) > 12;
    if (iptr_req){
        ind_ptr = malloc(DEFAULT_BLOCK_SIZE);

        // if the indirect pointer has not been allocated
        if (fd_table[fileID].inode->indirectPointer == -1){
            fd_table[fileID].inode->indirectPointer = get_index();
            // Check if space is allocated properly
            if (fd_table[fileID].inode->indirectPointer == -1){
                printf("insufficient space\n");
                free(ind_ptr);
                return write_count;
            }
            // initialize the indirect pointer
            for (int i=0; i<DEFAULT_BLOCK_SIZE/sizeof(uint16_t); i++){
                ind_ptr[i] = -1;
            }
        } else { // an indirect pointer is already being used
            read_blocks(fd_table[fileID].inode->indirectPointer, 1, ind_ptr);
        }
    }

    while (length > 0){
        // make sure the writing can proceed
        if ( shift >= DEFAULT_BLOCK_SIZE/sizeof(uint16_t) + 12){
            printf("single file size limit reached\n");
            break;
        }

        // copy from origin buffer to temporary buffer
        if (rem > 0){ // first section to be written
            if (shift < 12){
                read_blocks(fd_table[fileID].inode->data_ptrs[shift], 1, tmp);
            } else { // other part of the program will make sure there's an indirect pointer allocated
                read_blocks(ind_ptr[shift-12], 1, tmp);
            }
        }

        if(length + rem <= DEFAULT_BLOCK_SIZE){ // if this is the last part of data
            memcpy(tmp + rem, buf, length);
            write_count+=length;
        } else {
            memcpy(tmp + rem, buf, DEFAULT_BLOCK_SIZE - rem);
            write_count += (DEFAULT_BLOCK_SIZE - rem);
        }

        // write to disk
        if (shift < 12){
            // allocate space if needed
            if (fd_table[fileID].inode->data_ptrs[shift] == -1){
                fd_table[fileID].inode->data_ptrs[shift] = get_index();
                // Check if space is allocated properly
                if (fd_table[fileID].inode->data_ptrs[shift] == -1){
                    printf("insufficient space\n");
                    break;
                }
            }
            write_blocks(fd_table[fileID].inode->data_ptrs[shift], 1, tmp);

        } else { // if indirect pointer need to be used

            // allocate space if needed
            if (ind_ptr[shift-12] == (uint16_t) -1){
                ind_ptr[shift-12] = get_index();
                // Check if space is allocated properly
                if (ind_ptr[shift-12] == (uint16_t) -1){
                    printf("insufficient space\n");
                    break;
                }
            }
            write_blocks(ind_ptr[shift-12], 1, tmp);
        }

        shift++;
        length -= (DEFAULT_BLOCK_SIZE - rem);
        buf += (DEFAULT_BLOCK_SIZE - rem);
        if (rem > 0) rem = 0;
    }

    free(tmp);
    if (iptr_req){
        // update the indirect pointer
        write_blocks(fd_table[fileID].inode->indirectPointer, 1, ind_ptr);
        free(ind_ptr);
    }

    // update the rw pointer
    fd_table[fileID].rwptr += write_count;
    // Adjust the size information about the updated file
    if (fd_table[fileID].inode->size < fd_table[fileID].rwptr){
        fd_table[fileID].inode->size = fd_table[fileID].rwptr;
    }

    // update the possible changes to global variables
    write_blocks(SUPER_BLOCK_N, INODES_BLOCK_N, in_table); // inodes table
    write_blocks(SUPER_BLOCK_N + INODES_BLOCK_N, DIR_BLOCK_N, rootDir); // directory entry
    write_blocks(NUM_BLOCKS-1, 1, free_bit_map); // free bitmap

    return write_count;
}

int sfs_fseek(int fileID, int loc) {
    if (fd_table[fileID].inodeIndex == -1){
        printf("file not found in file descriptor (fseek)\n");
        return -1;
    }
    // Make sure the new location is not out of bound
    if (fd_table[fileID].inode->size < loc){
        printf("read/write pointer location requested out of bound\n");
        return -1;
    }
    fd_table[fileID].rwptr = loc;
    return 0;
}

int sfs_remove(char *file) {
    int current = 0;
    while (current < NUM_INODES){
        if (strncmp(file, rootDir[current].name, MAX_FILE_NAME)==0){
            int blocks_occ = BLOCK_REQ(in_table[rootDir[current].num].size);

            if (blocks_occ>12){ // clear the indirect pointer section first
                uint16_t *ind_ptr = malloc(DEFAULT_BLOCK_SIZE);
                read_blocks(in_table[rootDir[current].num].indirectPointer, 1, ind_ptr);
                while(blocks_occ>12){
                    rm_index(ind_ptr[blocks_occ-12-1]);
                    blocks_occ--;
                }
                rm_index(in_table[rootDir[current].num].indirectPointer);
                in_table[rootDir[current].num].indirectPointer = -1;
                free(ind_ptr);
            }
            for (int i=0; i<blocks_occ; i++){
                rm_index(in_table[rootDir[current].num].data_ptrs[i]);
                in_table[rootDir[current].num].data_ptrs[i] = -1;
            }

            in_table[rootDir[current].num].size = -1;
            rootDir[current].num = -1;
            rootDir[current].name[0] = '\0';
            // update the global variables
            write_blocks(SUPER_BLOCK_N, INODES_BLOCK_N, in_table); // inodes table
            write_blocks(SUPER_BLOCK_N + INODES_BLOCK_N, DIR_BLOCK_N, rootDir); // directory entry
            write_blocks(NUM_BLOCKS-1, 1, free_bit_map); // free bitmap
            return 0;
        }
        current++;
    }
    printf("Attempted to remove a file that does not exist\n");
    return -1;
}
