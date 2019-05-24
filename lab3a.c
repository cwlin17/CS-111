// NAME: Carol Lin
// EMAIL: carol9gmail@yahoo.com
// ID: 804984337

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include "ext2_fs.h"

const int SUPER_BLOCK_OFFSET = 1024;
int blockSize;

struct ext2_super_block superBlock;
struct ext2_group_desc groupDesc;
struct ext2_inode inode;
struct ext2_dir_entry dirEntry;

int main(int argc, char* argv[]){
  int i = 0;
  int j = 0;
  if(argc != 2){
    fprintf(stderr, "Wrong number of arguments.\n");
    exit(1);
  }

  int fd = open(argv[1], O_RDONLY);
  if(fd == -1){
    fprintf(stderr, "Error opening file.\n");
    exit(2);
  }

  // Getting superblock information
  if(pread(fd, &superBlock, sizeof(superBlock), SUPER_BLOCK_OFFSET) == -1){
    fprintf(stderr, "Error reading from file descriptor.\n");
    exit(2);
  }

  // Getting block size
  blockSize = 1024 << superBlock.s_log_block_size;

  printf("SUPERBLOCK,%d,%d,%d,%d,%d,%d,%d\n", superBlock.s_blocks_count, superBlock.s_inodes_count, blockSize, 
	 superBlock.s_inode_size, superBlock.s_blocks_per_group, superBlock.s_inodes_per_group, superBlock.s_first_ino);

  // Getting group descriptor table information
  if(pread(fd, &groupDesc, sizeof(groupDesc), SUPER_BLOCK_OFFSET + blockSize) == -1){
    fprintf(stderr, "Error reading from file descriptor.\n");
    exit(2);
  } 

  // Calculating # of blocks in group assuming there's only 1 group
  int blocksPerGroup = superBlock.s_blocks_per_group;
  int totalBlocks = superBlock.s_blocks_count;
  if(blocksPerGroup > totalBlocks){
    blocksPerGroup = totalBlocks;
  }

  // Calculating # of inodes in groups assuming there's only 1 group
  int inodesPerGroup = superBlock.s_inodes_per_group;
  int totalInodes = superBlock.s_inodes_count;
  if(inodesPerGroup > totalInodes){
    inodesPerGroup = totalInodes;
  }

  printf("GROUP,0,%d,%d,%d,%d,%d,%d,%d\n", blocksPerGroup, inodesPerGroup, groupDesc.bg_free_blocks_count, 
	 groupDesc.bg_free_inodes_count, groupDesc.bg_block_bitmap, groupDesc.bg_inode_bitmap,
	 groupDesc.bg_inode_table);


  /////////////////////////////////////////////////////////////////
  //Find free blocks with bit map
  __u8 blockMapByte;
  i = 0;
  j = 0;
  for (; i < blocksPerGroup; j++, i += 8) { //For each block
    pread(fd, &blockMapByte, 1, (EXT2_MIN_BLOCK_SIZE << superBlock.s_log_block_size) * groupDesc.bg_block_bitmap + j);
    int n = 0;
    for (; n < 8; n++){
      if ((blockMapByte & (1 << n)) == 0){
	printf("BFREE, %i\n", i+n+1); //blocks start at 1 on map not 0 
      }
    }
  }


  exit(0);
}
