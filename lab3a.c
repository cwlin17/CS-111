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

int getOffset(int blockID){
  return SUPER_BLOCK_OFFSET + (blockID - 1) * blockSize;
}
int main(int argc, char* argv[]){
  unsigned int i = 0;
  unsigned int j = 0;

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

  /************ Free Inode Entries *************/
  int readBytes = inodesPerGroup / 8;
  // Read one extra byte if number of bits isn't a multiple of 8
  if(inodesPerGroup % 8 != 0){
    readBytes += 1;
  }
  char inodeBits[readBytes];
  if(pread(fd, &inodeBits, readBytes, groupDesc.bg_inode_bitmap) == -1){
    fprintf(stderr, "Error reading from file descriptor.\n");
    exit(2);
  }
  i = 0;
  j = 0;
  for(; i < readBytes; i++){
    char c = inodeBits[i];
    for(; j < 8; j++){
      if((c & (1 << j)) == 0){
	printf("IFREE,%d\n", (i * 8) + j + 1); // First bit represents 1st inode
      }
    }
  }

  /////////////////////////////////////////////////////////////////
  //Find free blocks with bit map
  __u8 blockMapByte;
  i = 0;
  j = 0;
  for (; i < (unsigned int)blocksPerGroup; j++, i += 8) { //For each block
    pread(fd, &blockMapByte, 1, (EXT2_MIN_BLOCK_SIZE << superBlock.s_log_block_size) * groupDesc.bg_block_bitmap + j);
    int n = 0;
    for (; n < 8; n++){
      if ((blockMapByte & (1 << n)) == 0){
	printf("BFREE, %i\n", i+n+1); //blocks start at 1 on map not 0 
      }
    }
  }

  ///////////////////////////////////////////////////////////////////
  //Looking at the Inode Table
  //  printf("This is the inode:%i\n", groupDesc.bg_inode_table);
  // printf("This is the calculated:%lu\n", SUPER_BLOCK_OFFSET + blockSize * 4);
  i = 0;
  struct ext2_inode inodeEntry;
  for (; i < superBlock.s_inodes_per_group; i++){ //For each inode in table
    pread(fd, &inodeEntry, sizeof(inodeEntry), i*sizeof(inodeEntry) + getOffset(groupDesc.bg_inode_table)); //How come groupDesc.bg_inode_bitmap

    __u16 i_modeVal = inodeEntry.i_mode;
    char fileType= 'n';
    
    if (i_modeVal & 0x8000)
      fileType = 'f';
    else if (i_modeVal & 0x4000)
      fileType = 'd';
    else if (i_modeVal & 0xA000)
      fileType = 's';
    printf("INODE,%i,", i+1);
    printf("%c,", fileType);
    printf("%o,", i_modeVal & 0XFFF); //Mode
    printf("%d,", inodeEntry.i_uid); //Owner
    printf("%d,", inodeEntry.i_gid); //group
    printf("%d,", inodeEntry.i_links_count); //link count
    printf("\n");
  }

  exit(0);
}
