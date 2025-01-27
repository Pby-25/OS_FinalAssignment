#ifndef _INCLUDE_BITMAP_H_
#define _INCLUDE_BITMAP_H_

#include <stdint.h>

#define NUM_BLOCKS 1024
#define BITMAP_ROW_SIZE (NUM_BLOCKS/8) // this essentially mimcs the number of rows we have in the bitmap. we will have 128 rows.

extern uint8_t free_bit_map[BITMAP_ROW_SIZE];

/*
 * @short force an index to be set.
 * @long Use this to setup your superblock, inode table and free bit map
 *       This has been left unimplemented. You should fill it out.
 *
 * @param index index to set
 *
 */
void force_set_index(uint32_t index);

/*
 * @short find the first free data block
 * @return index of data block to use
 */
uint32_t get_index();

/*
 * @short frees an index
 * @param index the index to free
 */
void rm_index(uint32_t index);

#endif //_INCLUDE_BITMAP_H_
