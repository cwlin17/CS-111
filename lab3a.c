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

int main(int argc, char* argv[]){
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
  if(pread(fd, superBlock, sizeof(superBlock), SUPER_BLOCK_OFFSET) == -1){
    fprintf(stderr, "Error reading from file descriptor.\n");
    exit(2);
  }

  // Getting block size
  blockSize = 1024 << superBlock.s_log_block_size;

  printf("SUPERBLOCK,%d,%d,%d,%d,%d,%d,%d\n", superBlock.s_blocks_count, superBlock.s_inodes_count, blockSize, 
	 superBlock.s_inode_size, superBlock.s_blocks_per_group, superBlock.s_inodes_per_group, superBlock.s_first_ino);

  exit(0);
}
