//
//  main.c
//  lab3A
//
//  Created by Mike Nourian on 3/2/18.
//  Copyright © 2018 Mike Nourian. All rights reserved.
//

#include <getopt.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "ext2_fs.h"
#include <time.h>

#define BLOCK_OFFSET 1024
//Globals
char *imageFileName = NULL;
int fd = 0;
struct ext2_super_block superBuffer;
int groupsCount = 0; //to record the total number of groups
int blockSize = 0;
int totalBlocks = 0;
int totalInodes = 0;
int inodeSize = 0;
struct ext2_group_desc *groupBuffer = NULL;
int inodesPerGroup = 0;

//end of globals

//helper functions

/*
 ssize_t pread(int fd, void *buf, size_t count, off_t offset);
 */

void handleError(char *str)
{
    fprintf(stderr, "Error in function: %s\n", str);
    exit(EXIT_FAILURE);
}
void handleSuperBlock()
{
    //superblock starts at offset 1024
    ssize_t rc = pread(fd, &superBuffer, sizeof(struct ext2_super_block), 1024);
    if (!rc)
    {
        fprintf(stderr, "%s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    inodeSize = superBuffer.s_inode_size;
    inodesPerGroup = superBuffer.s_inodes_per_group;
    blockSize = 1024 << superBuffer.s_log_block_size;
    totalBlocks = superBuffer.s_blocks_count;
    totalInodes = superBuffer.s_inodes_count;
    fprintf(stdout, "SUPERBLOCK,%u,%u,%u,%u,%u,%u,%u\n", superBuffer.s_blocks_count, superBuffer.s_inodes_count, blockSize, superBuffer.s_inode_size, superBuffer.s_blocks_per_group, superBuffer.s_inodes_per_group, superBuffer.s_first_ino);
}
void handleGroup()
{
    groupsCount = 1 + ((superBuffer.s_blocks_count - 1) / superBuffer.s_blocks_per_group);
    //for each group, we know to allocate space to hold ext2_group_desc
    if ((groupBuffer = malloc(groupsCount * sizeof(struct ext2_group_desc))) == NULL)
    {
        fprintf(stderr, "Error in malloc, exiting with failure\n");
        exit(EXIT_FAILURE);
    }
    //This would be the third block on a 1KiB block file system, or the second block for 2KiB and larger block file systems.
    ssize_t rc = pread(fd, groupBuffer, sizeof(struct ext2_group_desc) * groupsCount, 1024 + blockSize); //offset = superbOffset+ blocksize
    if (!rc)
    {
        fprintf(stderr, "Error with pread in handleGroup, %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    //we know total number of blocks found from superblock, has to be used by the groups
    unsigned int num_left_blocks = totalBlocks;
    unsigned int num_left_inodes = totalInodes;
    int i = 0;
    for (; i < groupsCount; i++)
    { //now process each group and output the info for each
        int blockCount = (num_left_blocks < superBuffer.s_blocks_per_group) ? num_left_blocks : superBuffer.s_blocks_per_group;
        int inodeCount = (num_left_inodes < superBuffer.s_inodes_per_group) ? num_left_inodes : superBuffer.s_inodes_per_group;
        fprintf(stdout, "GROUP,%u,%u,%u,%u,%u,%u,%u,%u\n", i, blockCount, inodeCount, groupBuffer[i].bg_free_blocks_count, groupBuffer[i].bg_free_inodes_count, groupBuffer[i].bg_block_bitmap, groupBuffer[i].bg_inode_bitmap, groupBuffer[i].bg_inode_table);
        num_left_blocks -= superBuffer.s_blocks_per_group;
        num_left_inodes -= superBuffer.s_inodes_per_group;
    }
}

void handleFreeBlock()
{
    __u8 *blockBuff;
    blockBuff = malloc(blockSize);
    if (blockBuff == NULL)
    {
        handleError("malloc in handleFreeBlock\n");
    }
    for (int i = 0; i < groupsCount; i++)
    {
        if (pread(fd, blockBuff, blockSize, groupBuffer[i].bg_block_bitmap * blockSize) < 0)
        { //block bitmap is an id
            handleError("pread in handleFreeBlock");
        }
        //now we process the in
        //process each bytes
        for (int j = 0; j < blockSize; j++)
        {
            //parse with a mask
            //1 represents used, 0 represents not used
            int bitmask = 0x1;
            __u8 singleByte = blockBuff[j];
            int k = 0;
            for (; k < 8; k++)
            {
                if ((bitmask & singleByte) == 0)
                {
                    fprintf(stdout, "BFREE,%u\n", (k + 1) + (8 * j) + i * superBuffer.s_blocks_per_group);
                }
                bitmask = bitmask << 1;
            }
        }
    }
    free(blockBuff);
}

/*
 free I-node entries
 Scan the free I-node bitmap for each group. For each free I-node, produce a new-line terminated line, with two comma-separated fields (with no white space).
 1. IFREE
 2. numberofthefreeI-node(decimal)
*/

/*
 3.4. Inode Bitmap
 The "Inode Bitmap" works in a similar way as the "Block Bitmap", difference being in each bit representing an inode in the "Inode Table" rather than a block.
 
 There is one inode bitmap per group and its location may be determined by reading the "bg_inode_bitmap" in its associated group descriptor.
 
 When the inode table is created, all the reserved inodes are marked as used. In revision 0 this is the first 11 inodes.
*/
void freeInodes()
{
    __u8 *inodeBuff;
    inodeBuff = malloc(blockSize);
    if (inodeBuff == NULL)
    {
        handleError("malloc in freeInodes");
    }
    int i = 0;
    for (; i < groupsCount; i++)
    {
        if (pread(fd, inodeBuff, blockSize, blockSize * groupBuffer[i].bg_inode_bitmap) < 0)
        {
            handleError("pread in freeInodes");
        }
        //now we have read the inode bitmap block (a bit map has to fit within 1 block)
        int j = 0;
        for (; j < blockSize; j++)
        {
            //process each bytes seperately
            uint bitmask = 0x1;
            int k = 0;
            __u8 singleByte = inodeBuff[j];
            for (; k < 8; k++)
            {
                if ((singleByte & bitmask) == 0)
                {
                    fprintf(stdout, "IFREE,%u\n", i * superBuffer.s_blocks_per_group + (8 * j) + (k + 1));
                }
                bitmask = bitmask << 1;
            }
        }
    }
    free(inodeBuff);
}
/*
INODE
inodenumber(decimal)
filetype('f'forfile,'d'fordirectory,'s'forsymboliclink,'?"foranythingelse) 4. mode(loworder12-bits,octal...suggestedformat"0%o")
owner(decimal)
group(decimal)
linkcount(decimal)
timeoflastI-nodechange(mm/dd/yyhh:mm:ss,GMT)
modificationtime(mm/dd/yyhh:mm:ss,GMT)
time of last access (mm/dd/yy hh:mm:ss, GMT)
file size (decimal)
number of blocks (decimal)
 */

void handle_DIRECT_output(char *directBuff, int parentInodeNum)
{
    int processed_size = 0;
    struct ext2_dir_entry *dir_ent_ptr = (struct ext2_dir_entry *)directBuff;
    //rec_len = 16bit unsigned displacement to the next directory entry from the start of the current directory entry.
    while (processed_size < blockSize && dir_ent_ptr->file_type)
    { //check that we have the correct size and that the data is actually valid
        char fileName[dir_ent_ptr->name_len + 1];
        for (int i = 0; i < dir_ent_ptr->name_len; i++)
        { //could use memcpy
            fileName[i] = dir_ent_ptr->name[i];
        }
        fileName[dir_ent_ptr->name_len] = 0; //null terminated string
        if (dir_ent_ptr != NULL)
        {
            fprintf(stdout, "DIRENT,%d,%u,%u,%u,%u,'%s'\n", parentInodeNum, processed_size, dir_ent_ptr->inode, dir_ent_ptr->rec_len, dir_ent_ptr->name_len, fileName);
        }
        processed_size = processed_size + dir_ent_ptr->rec_len;
        dir_ent_ptr = dir_ent_ptr->rec_len + (void *)dir_ent_ptr;
    }
}

void handle_DIRECT_DirectoryEntries(struct ext2_inode *inode_for_directory, int parentInodeNum)
{
    int max_index = inode_for_directory->i_blocks / (2 << superBuffer.s_log_block_size);
    int i = 0;
    for (; i < max_index + 1; i++)
    {
        //        if ( i >= 12 && i <= 14 && inode_for_directory->i_block[i]){ //check if we have to handle the inderct blocks
        //            handle_INDIRECT_DirectoryEntries(inode_for_directory, i - 11, parentInodeNum);
        //        }
        if (i > 14)
        {
            handleError("handle_DIRECT had index larger that 14");
        }
        //otherwise we have direct blocks that we can process by reading
        //read the entire block of the inode and then process each
        char directBuff[blockSize];
        //#define BLOCK_OFFSET(block) (BASE_OFFSET + (inode->i_block[i]-1) * block_size)
        //BLOCK_OFFSET()
        ssize_t rc = pread(fd, &directBuff, blockSize, 1024 + blockSize * (inode_for_directory->i_block[i] - 1));
        if (rc <= 0)
        {
            handleError("pread in handle_DIRECT");
        }
        //now that we have read the entire block
        handle_DIRECT_output(directBuff, parentInodeNum);
    }
}

int firstLevelCount = 0;

void indirectBlockRef(struct ext2_inode *inode, int parentInodeNum, int firstIndirectBlock, int secondIndirectBlock, int thirdIndirectBlock, int offset)
{
    __u8 *blockBuff;
    blockBuff = malloc(blockSize);
    if (blockBuff == NULL)
    {
        handleError("malloc in indirectBlockRef\n");
    }

    if (firstIndirectBlock)
    {
        if (pread(fd, blockBuff, blockSize, blockSize * firstIndirectBlock) < 0)
        {
            handleError("pread in indirectBlockRef");
        }

        int i = 0;

        int count = 12 + firstLevelCount;

        if (offset)
        {
            count = offset;
            count += firstLevelCount - 1;
        }

        for (; i < blockSize; i++)
        {
            if (blockBuff[i] != 0)
            {
                fprintf(stdout, "INDIRECT,%u,%u,%u,%u,%u\n", parentInodeNum, 1, count, firstIndirectBlock, blockBuff[i]);
                count++;

                if (!offset)
                {
                    firstLevelCount++;
                }
                
            }
        }
    }

    if (secondIndirectBlock)
    {
        if (pread(fd, blockBuff, blockSize, blockSize * secondIndirectBlock) < 0)
        {
            handleError("pread in indirectBlockRef");
        }

        int i = 0;

        int count = 268;

        if (offset)
        {
            count = offset;
        }

        for (; i < blockSize; i++)
        {

            if (blockBuff[i] != 0)
            {
                fprintf(stdout, "INDIRECT,%u,%u,%u,%u,%u\n", parentInodeNum, 2, count, secondIndirectBlock, blockBuff[i]);
                indirectBlockRef(inode, parentInodeNum, blockBuff[i], 0, 0, count);
                count = count + 256;
            }
        }
    }

    if (thirdIndirectBlock)
    {
        if (pread(fd, blockBuff, blockSize, blockSize * thirdIndirectBlock) < 0)
        {
            handleError("pread in indirectBlockRef");
        }

        int i = 0;

        int count = 65804;

        if (offset)
        {
            count = offset;
        }

        for (; i < blockSize; i++)
        {

            if (blockBuff[i] != 0)
            {
                fprintf(stdout, "INDIRECT,%u,%u,%u,%u,%u\n", parentInodeNum, 3, count, thirdIndirectBlock, blockBuff[i]);
                indirectBlockRef(inode, parentInodeNum, 0, blockBuff[i], 0, count);
                count = count + 65536;
            }
        }
    }

    free(blockBuff);
}

void handleInodes()
{
    struct ext2_inode *inodeTableBuff;
    int inodeTableSize = inodesPerGroup * inodeSize; // there are s_inodes_per_group inodes per table
    inodeTableBuff = malloc(inodeTableSize);         // make a buffer to read in the inode table for each group

    // check to make sure there were no malloc errors
    if (inodeTableBuff == NULL)
    {
        handleError("malloc in handleInodes");
    }

    // iterate through each group
    int i = 0;
    for (; i < groupsCount; i++)
    {
        // read the inode table for each group
        if (pread(fd, inodeTableBuff, inodeTableSize, blockSize * groupBuffer[i].bg_inode_table) < 0)
        {
            handleError("pread in handleInodes");
        }

        // iterate through the inode table and check if each inode is allocated
        //int j = 0;
        int j = 0;
        for (; j < inodesPerGroup; j++)
        {
            struct ext2_inode *inode = &inodeTableBuff[j];
            int inodeNum = j + 1;

            // check that the current inode is allocated
            if (inode->i_mode != 0x0000 && inode->i_links_count != 0x0000)
            {
                // check the file type of the inode
                // if it's a directory, call the handleDirectoryEntries function

                char fileType = '?';

                // check if mode is either a file or symlink
                if (inode->i_mode & 0x8000)
                {
                    // symlink case
                    if (inode->i_mode & 0x2000)
                    {
                        fileType = 's';
                    }

                    // file case
                    else
                    {
                        fileType = 'f';
                        indirectBlockRef(inode, inodeNum, inode->i_block[12], inode->i_block[13], inode->i_block[14], 0);
                    }
                }

                // check if mode is a directory
                else if (inode->i_mode & 0x4000)
                {
                    fileType = 'd';
                    // call directory entries function here
                    //Mike: added this function so this gets called everytime we find a directory in the inode table
                    indirectBlockRef(inode, inodeNum, inode->i_block[12], inode->i_block[13], inode->i_block[14], 0);
                    handle_DIRECT_DirectoryEntries(inode, j + 1); //Mike added
                }

                // convert the creation, modification, and access time from epoch to correct format
                char creationTime[32];
                char modificationTime[32];
                char accessTime[32];

                struct tm ts;

                time_t c_time = inode->i_ctime;
                time_t m_time = inode->i_mtime;
                time_t a_time = inode->i_atime;

                ts = *gmtime(&c_time);
                strftime(creationTime, sizeof(creationTime), "%D %T", &ts);

                ts = *gmtime(&m_time);
                strftime(modificationTime, sizeof(modificationTime), "%D %T", &ts);

                ts = *gmtime(&a_time);
                strftime(accessTime, sizeof(accessTime), "%D %T", &ts);

                // print out message if inode is allocated

                int inodeNum = j + 1;

                // first 12 fields of the output (INODE,inode number, file type, mode, owner, group, link count, time of creation, modification time, time of last access, file size, number of blocks)
                fprintf(stdout, "INODE,%u,%c,%o,%u,%u,%u,%s,%s,%s,%u,%u,", inodeNum, fileType, (inode->i_mode) & 0x0FFF, inode->i_uid, inode->i_gid, inode->i_links_count, creationTime, modificationTime, accessTime, inode->i_size, inode->i_blocks);

                // next 15 fields of the output (block addresses, 12 direct, one indirect, one double indirect, one triple indirect)
                fprintf(stdout, "%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n", inode->i_block[0], inode->i_block[1], inode->i_block[2], inode->i_block[3], inode->i_block[4], inode->i_block[5], inode->i_block[6], inode->i_block[7], inode->i_block[8], inode->i_block[9], inode->i_block[10], inode->i_block[11], inode->i_block[12], inode->i_block[13], inode->i_block[14]);
            }
        }
    }

    free(inodeTableBuff);
}

//void handleInodes(){
//    struct ext2_inode inode;
//    int i =0;
////#define BLOCK_OFFSET(block) (BASE_OFFSET + (block-1) * block_size)
//
//    int offset = BLOCK_OFFSET+(groupBuffer->bg_inode_table -1) * blockSize + sizeof(struct ext2_inode); //since the root directory inode = 2 and so its offset sizeof(ext2_inodes)
//    for (; i < groupsCount; i++){//go through every group
//        //go through the inodes and find out if the mode is 0X0 or not and if link count > 0
//        int j = 2; //start with the root
//        for (; j < superBuffer.s_inodes_per_group; j++){
//            if(pread(fd, &inode, sizeof(struct ext2_inode), offset)<0){
//                handleError("pread in handleNodes");
//            }
//            if (inode.i_mode == 0 || inode.i_links_count <= 0){
//                continue; //dont do anything if the inode is not in use
//            }
//
//            offset += sizeof(struct ext2_inode);
//        }
//
////        if (inode.i)
//
//    }
//}

//struct ext2_dir_entry {
//    __u32    inode;            /* Inode number */
//    __u16    rec_len;        /* Directory entry length */
//    __u8    name_len;        /* name length    */
//    __u8    file_type;        /* file type */
//    char    name[EXT2_NAME_LEN];    /* File name */
//};

//void handleDirectoryEntries(){
//    //read inode of th
//    int i = 0;
//    for (; i < groupsCount; i++){//for each group, read the
//        //also for each of the directories
//
//        __u8 * inodeBuff = malloc(superBuffer.s_inode_size); //to read in the values
//        if (pread(fd, inodeBuff, superBuffer.s_inode_size, blockSize * groupBuffer->bg_inode_table) < 0){ //this is the first
//            handleError("pread in handleDirectoryEntries");
//        }
//
//
//        //bg_inode * blocksize
//        //read the size of the inode
//
//        if(inodeBuff == NULL){
//            handleError("malloc in handleDirecoryEntries");
//        }
//        //if (pread(fd, inodeBuff, <#size_t __nbyte#>, <#off_t __offset#>))
//    }
//}

void handleExiting()
{
    //free groupBuffer
}
//end of helper functions

int main(int argc, char **argv)
{
    // insert code here...
    if (argc != 2)
    {
        fprintf(stderr, "Usage Error\n");
        exit(EXIT_FAILURE);
    }

    imageFileName = argv[1]; //uncomment
    fd = open(imageFileName, O_RDONLY);
    if (fd < 0)
    {
        fprintf(stderr, "issue opening the file, exiting, %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    //now call the functions
    handleSuperBlock(); //Mike
    handleGroup();      //Mike
    handleFreeBlock();  //Mike
    freeInodes();       //Mike
    handleInodes();     //Charlotte
                        //    handleDirectoryEntries(); //Mike
    //indirectBlockRef(); //Charlotte

    handleExiting();

    return 0;
}
