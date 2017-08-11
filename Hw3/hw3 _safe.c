#include <fcntl.h>
#include <stdio.h>
#include <cstdlib>
#include <unistd.h>
#include "ext2.h"
#include <vector>
#include <climits>
#define BASE_OFFSET 1024
#define EXT2_BLOCK_SIZE 1024
#define IMAGE "image_direct.img"

typedef unsigned char bmap;
#define __NBITS (8 * (int) sizeof (bmap))
#define __BMELT(d) ((d) / __NBITS)
#define __BMMASK(d) ((bmap) 1 << ((d) % __NBITS))
#define BM_SET(d, set) ((set[__BMELT (d)] |= __BMMASK (d)))
#define BM_CLR(d, set) ((set[__BMELT (d)] &= ~__BMMASK (d)))
#define BM_ISSET(d, set) ((set[__BMELT (d)] & __BMMASK (d)) != 0)

unsigned int block_size = 0;
#define BLOCK_OFFSET(block) (BASE_OFFSET + (block-1)*block_size)

struct inode_and_block{
    unsigned int non_reserved_inode_no;
    unsigned int block_no;
};
int main(void)
{
    struct ext2_super_block super;
    struct ext2_group_desc group;
    int fd;

    if ((fd = open(IMAGE, O_RDWR)) < 0) {
        perror(IMAGE);
        exit(1);
    }

    // read super-block
    lseek(fd, BASE_OFFSET, SEEK_SET);
    read(fd, &super, sizeof(super));
    if (super.s_magic != EXT2_SUPER_MAGIC) {
        fprintf(stderr, "Not a Ext2 filesystem\n");
        exit(1);
    }
    block_size = 1024 << super.s_log_block_size;

    
    // read group descriptor
    lseek(fd, BASE_OFFSET + block_size, SEEK_SET);
    read(fd, &group, sizeof(group));
    // read block bitmap
    bmap *bitmap;
    bitmap =(bmap *) malloc(block_size);
    lseek(fd, BLOCK_OFFSET(group.bg_block_bitmap), SEEK_SET); //reads the block bitmap through using macro. Well done.
    read(fd, bitmap, block_size);
    
    


    //read inode bitmap
    bmap *ibitmap;
    ibitmap=(bmap *)malloc(block_size);
    lseek(fd, BLOCK_OFFSET(group.bg_inode_bitmap), SEEK_SET); //reads the block bitmap through using macro. Well done.
    read(fd, ibitmap, block_size);
    
    
    //Read all non-reserved & non-free inodes and determine all the blocks that are owned by them.
    std::vector<inode_and_block> v; //holds the required information for the block,inode pairs in a bundle.
    for(int i=super.s_first_ino;i<super.s_inodes_count;i++){
        struct ext2_inode inode;
        lseek(fd, BLOCK_OFFSET(group.bg_inode_table)+(sizeof(struct ext2_inode) * i), SEEK_SET); // +sizeof(struct ext2_inode) *i is done to read all reserved inodes consecutively.
        read(fd, &inode, sizeof(struct ext2_inode));
        if(inode.i_size>0){
            for(int j=0; j < 15; j++){
                if(inode.i_block[j]>0){
                    struct inode_and_block iab;
                    iab.non_reserved_inode_no = i+1;
                    iab.block_no = inode.i_block[j];
                    
                    if (BM_ISSET(i,ibitmap)){
                        v.push_back(iab);
                        if(j<12) 
                            printf("%d %d\n",i+1,inode.i_block[j]);
                        
                        else if(j==12){//indirect pointer
                            printf("%d %d\n",i+1,inode.i_block[j]);
                            unsigned int indirect_data_block[block_size/sizeof(unsigned int)];
                            lseek(fd, BLOCK_OFFSET(group.bg_inode_table)+(sizeof(struct ext2_inode) * super.s_inodes_count) + (block_size * (inode.i_block[j]-9)), SEEK_SET); // go to the indirect data pointer block.
                            read(fd,&indirect_data_block,block_size);
                            for(unsigned int x=0;x<block_size/sizeof(unsigned int);x++){
                                if(indirect_data_block[x]>0){
                                    struct inode_and_block ind_iab;
                                    ind_iab.non_reserved_inode_no = i+1;
                                    ind_iab.block_no = indirect_data_block[x];
                                    v.push_back(ind_iab);
                                    printf("%d %d\n",i+1,indirect_data_block[x]);
                                }
                            }
                        }
                        else if(j==13){//double indirect pointer
                            printf("%d %d\n",i+1,inode.i_block[j]);
                        }
                        else if(j==14){//triple indirect pointer
                            printf("%d %d\n",i+1,inode.i_block[j]);
                        }
                    }
                }
            }
        }
    }
    printf("###\n");
    

    /* Find the lowest possible block:
     * 1-Check the bitmap and find first empty block.
     * 2-Check the v vector that holds info about non-reserved inodes' blocks.
     * 3-If 1<2, then place current block of inode to that empty block directly and update the bitmap
     * 4-Else, make a swap operation. Update inodes.
    */

    
    //Init defragmentation
    for(int i=super.s_first_ino;i<super.s_inodes_count;i++){
        struct ext2_inode inode;
        lseek(fd, BLOCK_OFFSET(group.bg_inode_table)+(sizeof(struct ext2_inode) * i), SEEK_SET); 
        read(fd, &inode, sizeof(struct ext2_inode));
        /*for(int l=super.s_first_ino;l<super.s_inodes_count;l++){
            struct ext2_inode inodex;
            lseek(fd, BLOCK_OFFSET(group.bg_inode_table)+(sizeof(struct ext2_inode) * l), SEEK_SET); // +sizeof(struct ext2_inode) *i is done to read all reserved inodes consecutively.
            read(fd, &inodex, sizeof(struct ext2_inode));
            if(inodex.i_size>0){
                for(int j=0; j < 15; j++){
                    if(inodex.i_block[j]>0){
                        if (BM_ISSET(l,ibitmap))
                            printf("%d %d\n",l+1,inodex.i_block[j]);
                    }
                }
            }
        }*/
        if(inode.i_size>0 && BM_ISSET(i,ibitmap)){
            for(int j=0; j < 15; j++){
                if(inode.i_block[j]>0){
                    unsigned int lowest_free_block;
                    unsigned int lowest_non_reserved_block = UINT_MAX;//Max unsigned int
                    unsigned int corresponding_inode = UINT_MAX;    
                    /*---------------------------------------1-----------------------------*/
                    for (int k = 0; k < super.s_blocks_count; k++){
                        if (!BM_ISSET(k,bitmap)){
                            lowest_free_block = k+1;
                            break;
                        }                           
                    }
                    /*---------------------------------------------------------------------*/
                    /*---------------------------------------2-----------------------------*/
                    for (std::vector<inode_and_block>::iterator it = v.begin() ; it != v.end(); ++it){
                         struct inode_and_block hold = *it;
                         if(hold.block_no < lowest_non_reserved_block)
                         {
                            lowest_non_reserved_block = hold.block_no;
                            corresponding_inode = hold.non_reserved_inode_no;
                         }
                    }
                    
                    //printf("lowest_non_reserved_block is %u, corresponding_inode is %u\n",lowest_non_reserved_block,corresponding_inode);
                    /*---------------------------------------------------------------------*/
                    /*---------------------------------------3-----------------------------*/
                    if(lowest_free_block < lowest_non_reserved_block){
                        /*printf("1-lowest free block is %u, lowest_non_reserved_block is %u\n",lowest_free_block,lowest_non_reserved_block);
                        printf("Trying to update inode %u\n",i+1);*/
                        //place current block of inode to that empty block directly and update the bitmap
                        if(j<12){
                            char data_block[block_size];
                            lseek(fd, BLOCK_OFFSET(group.bg_inode_table)+(sizeof(struct ext2_inode) * super.s_inodes_count) + (block_size * (inode.i_block[j]-1)), SEEK_SET); // go to the to be placed data block
                            read(fd,&data_block,block_size); //read the to be placed data block.
                            lseek(fd,block_size * -1,SEEK_CUR);//return to the beginning of block after read.
                            write(fd,0,block_size); //zero out the memory.
                            
                            lseek(fd, BLOCK_OFFSET(group.bg_inode_table)+(sizeof(struct ext2_inode) * super.s_inodes_count) + (block_size * (lowest_free_block-1)), SEEK_SET); // go to the placee data block
                            write(fd,&data_block,block_size);//place the data block.
                            BM_CLR(inode.i_block[j]-1,bitmap);//old block is now free
                            inode.i_block[j]=lowest_free_block;
                            BM_SET(lowest_free_block-1,bitmap);//new block is now used
                            
                        }
                        else if(j==12){

                            char data_block[block_size];
                            lseek(fd, BLOCK_OFFSET(group.bg_inode_table)+(sizeof(struct ext2_inode) * super.s_inodes_count) + (block_size * (inode.i_block[j]-9)), SEEK_SET); // go to the to be placed data block
                            read(fd,&data_block,block_size); //read the to be placed data block.
                            lseek(fd,block_size * -1,SEEK_CUR);//return to the beginning of block after read.
                            write(fd,0,block_size); //zero out the memory.
                            lseek(fd, BLOCK_OFFSET(group.bg_inode_table)+(sizeof(struct ext2_inode) * super.s_inodes_count) + (block_size * (lowest_free_block-1)), SEEK_SET); // go to the placee data block
                            write(fd,&data_block,block_size);//place the data block.

                            BM_CLR(inode.i_block[j]-9,bitmap);//old block is now free
                            inode.i_block[j]=lowest_free_block;
                            BM_SET(lowest_free_block-1,bitmap);//new block is now used
                            
                        }
                    }
                    /*---------------------------------------------------------------------*/
                    else{
                        //printf("2-lowest free block is %u, lowest_non_reserved_block is %u\n",lowest_free_block,lowest_non_reserved_block);
                        char data_block_tbp[block_size];//to be placed data block
                        char data_block_bp[block_size];//placee data block.
                        unsigned int swap_block;
                        struct ext2_inode inode_r;
                        lseek(fd, BLOCK_OFFSET(group.bg_inode_table)+(sizeof(struct ext2_inode) * super.s_inodes_count) + (block_size * (inode.i_block[j]-1)), SEEK_SET); // go to the to be placed data block
                        read(fd,&data_block_tbp,block_size); //read the to be placed data block.

                        lseek(fd, BLOCK_OFFSET(group.bg_inode_table)+(sizeof(struct ext2_inode) * super.s_inodes_count) + (block_size * (lowest_non_reserved_block-1)), SEEK_SET); // go to the placee data block
                        read(fd,&data_block_bp,block_size);//read the placee data block.
                        lseek(fd,block_size * -1,SEEK_CUR);//return to the beginning of block after read.
                        write(fd,&data_block_tbp,block_size);//write tbp block to the new place.
                        
                        lseek(fd, BLOCK_OFFSET(group.bg_inode_table)+(sizeof(struct ext2_inode) * super.s_inodes_count) + (block_size * (inode.i_block[j]-1)), SEEK_SET); // go to the to be placed data block
                        write(fd,&data_block_bp,block_size);//write bp block to the old place.

                        swap_block=inode.i_block[j];
                        inode.i_block[j]=lowest_non_reserved_block; //update tbp inode.
                        /* --------------------------update own table---------------------------------------*/
                        unsigned int z=0;
                        /*for (std::vector<inode_and_block>::iterator it = v.begin() ; it != v.end(); ++it,++z){
                             struct inode_and_block hold = *it;
                             printf("(BEFORE)%d -> inode : %u , block : %u\n",z,hold.non_reserved_inode_no,hold.block_no);
                        }
                        z=0;*/
                        for (std::vector<inode_and_block>::iterator it = v.begin() ; it != v.end(); ++it,++z){
                             struct inode_and_block hold = *it;
                             if(hold.non_reserved_inode_no == i+1 && hold.block_no==swap_block)
                             {
                                //printf("inode %d has block %d (before)\n",hold.non_reserved_inode_no,hold.block_no);

                                hold.block_no=lowest_non_reserved_block;
                                //printf("inode %d has block %d (after)\n",hold.non_reserved_inode_no,hold.block_no);
                                *it = hold;
                                break; //update table

                                
                             }          
                        }
                        for (std::vector<inode_and_block>::iterator it = v.begin() ; it != v.end(); ++it){
                             struct inode_and_block hold = *it;
                             if(hold.block_no == lowest_non_reserved_block && hold.non_reserved_inode_no == corresponding_inode){
                                //printf("inode %d has block %d (before)\n",hold.non_reserved_inode_no,hold.block_no);
                                hold.block_no=swap_block;
                                /*printf("lowest_non_reserved_block is %u, corresponding_inode is %u,block_no is %u\n",lowest_non_reserved_block,corresponding_inode,hold.block_no);
                    
                                printf("inode %d has block %d (after)\n",hold.non_reserved_inode_no,hold.block_no);*/
                                *it=hold;
                                break;
                                
                             }
                        }
                        v.erase(v.begin()+z);
                        /*---------------------------update table done-------------------------------------*/
                        lseek(fd, BLOCK_OFFSET(group.bg_inode_table)+(sizeof(struct ext2_inode) * (corresponding_inode-1)), SEEK_SET); //find whose block is replaced inode.
                        read(fd,&inode_r,sizeof(struct ext2_inode));
                        for(int m=0;m<15;m++){
                            if(inode_r.i_block[m]==lowest_non_reserved_block){//find the previous pointer
                                inode_r.i_block[m] = swap_block; //update bp inode.
                                break;
                            }
                        }
                        lseek(fd,sizeof(struct ext2_inode) * -1,SEEK_CUR);//return to the beginning of block after read.
                        write(fd,&inode_r,sizeof(struct ext2_inode));
                        //update vector that holds information about non-reserved inode&block pairs.
                        BM_SET(lowest_non_reserved_block-1,bitmap);//new block is now used, normally already being used blocks would be swapped. But there are some exceptions that bypass this rule. So this statement is used as an "insurance".
                        
                    }

                    lseek(fd, BLOCK_OFFSET(group.bg_inode_table)+(sizeof(struct ext2_inode) * i), SEEK_SET); // go back again to inode.
                    write(fd,&inode,sizeof(struct ext2_inode));

                    if(j==12){
                        //printf("In indirect block, reaching to indirect block %u\n",inode.i_block[j]);

                        unsigned int indirect_data_block[block_size/sizeof(unsigned int)];
                        lseek(fd, BLOCK_OFFSET(group.bg_inode_table)+(sizeof(struct ext2_inode) * super.s_inodes_count) + (block_size * (inode.i_block[j]-1)), SEEK_SET); // go to the indirect data pointer block.
                        read(fd,&indirect_data_block,block_size);
                        

                        struct ext2_inode inode_r;
                        for(unsigned int x=0;x<block_size/sizeof(unsigned int);x++){
                            if(indirect_data_block[x]>0){
                                unsigned int lowest_free_block;
                                unsigned int lowest_non_reserved_block = UINT_MAX;//Max unsigned int
                                unsigned int corresponding_inode = UINT_MAX; 
                                for (int k = 0; k < super.s_blocks_count; k++){
                                    if (!BM_ISSET(k,bitmap)){
                                        lowest_free_block = k+1;
                                        break;
                                    }                           
                                } //lowest free block
                                for (std::vector<inode_and_block>::iterator it = v.begin() ; it != v.end(); ++it){
                                     struct inode_and_block hold = *it;
                                     if(hold.block_no < lowest_non_reserved_block)
                                     {
                                        lowest_non_reserved_block = hold.block_no;
                                        corresponding_inode = hold.non_reserved_inode_no;
                                     }
                                }//lowest_non_reserved_block
                                if(lowest_free_block < lowest_non_reserved_block){
                                    /*printf("1-lowest free block is %u, lowest_non_reserved_block is %u\n",lowest_free_block,lowest_non_reserved_block);
                                    printf("Trying to update inode %u\n",i+1);
                                    */
                                    char data_block[block_size];
                                    lseek(fd,BLOCK_OFFSET(group.bg_inode_table)+(sizeof(struct ext2_inode) * super.s_inodes_count) + (block_size * (indirect_data_block[x] - 1)),SEEK_SET);
                                    read(fd,&data_block,block_size); //read the to be placed data block.
                                    lseek(fd,block_size * -1,SEEK_CUR);//return to the beginning of block after read.
                                    write(fd,0,block_size);//zero out the memory.
                                    lseek(fd, BLOCK_OFFSET(group.bg_inode_table)+(sizeof(struct ext2_inode) * super.s_inodes_count) + (block_size * (lowest_free_block-1)), SEEK_SET); // go to the placee data block
                                    write(fd,&data_block,block_size);//place the data block.
                                    BM_CLR(indirect_data_block[x]-1,bitmap);//old block is now free
                                    indirect_data_block[x]=lowest_free_block;
                                    BM_SET(lowest_free_block-1,bitmap);//new block is now used
                                }
                                else{
                                    char data_block_tbp[block_size];//to be placed data block
                                    char data_block_bp[block_size];//placee data block.
                                    unsigned int swap_block;
                                    struct ext2_inode inode_r;
                                    lseek(fd, BLOCK_OFFSET(group.bg_inode_table)+(sizeof(struct ext2_inode) * super.s_inodes_count) + (block_size * (indirect_data_block[x] -1)), SEEK_SET); // go to the to be placed data block
                                    read(fd,&data_block_tbp,block_size); //read the to be placed data block.

                                    lseek(fd, BLOCK_OFFSET(group.bg_inode_table)+(sizeof(struct ext2_inode) * super.s_inodes_count) + (block_size * (lowest_non_reserved_block-1)), SEEK_SET); // go to the placee data block
                                    read(fd,&data_block_bp,block_size);//read the placee data block.
                                    lseek(fd,block_size * -1,SEEK_CUR);//return to the beginning of block after read.
                                    write(fd,&data_block_tbp,block_size);//write tbp block to the new place.
                                    
                                    lseek(fd, BLOCK_OFFSET(group.bg_inode_table)+(sizeof(struct ext2_inode) * super.s_inodes_count) + (block_size * (indirect_data_block[x]-1)), SEEK_SET); // go to the to be placed data block
                                    write(fd,&data_block_bp,block_size);//write bp block to the old place.
                                    swap_block=indirect_data_block[x];
                                    indirect_data_block[x]=lowest_non_reserved_block; //update tbp inode.
                                    /* --------------------------update own table---------------------------------------*/
                                    unsigned int z=0;
                                    /*for (std::vector<inode_and_block>::iterator it = v.begin() ; it != v.end(); ++it,++z){
                                         struct inode_and_block hold = *it;
                                         printf("(BEFORE)%d -> inode : %u , block : %u, swap block: %u\n",z,hold.non_reserved_inode_no,hold.block_no,swap_block);
                                    }*/
                                    z=0;
                                    for (std::vector<inode_and_block>::iterator it = v.begin() ; it != v.end(); ++it,++z){
                                         struct inode_and_block hold = *it;
                                         if(hold.non_reserved_inode_no == i+1 && hold.block_no==swap_block)
                                         {
                                            //printf("inode %d has block %d (before)\n",hold.non_reserved_inode_no,hold.block_no);

                                            hold.block_no=lowest_non_reserved_block;
                                            //printf("inode %d has block %d (after)\n",hold.non_reserved_inode_no,hold.block_no);
                                            *it = hold;
                                            break; //update table

                                            
                                         }          
                                    }
                                    for (std::vector<inode_and_block>::iterator it = v.begin() ; it != v.end(); ++it){
                                         struct inode_and_block hold = *it;
                                         if(hold.block_no == lowest_non_reserved_block && hold.non_reserved_inode_no == corresponding_inode){
                                            //printf("inode %d has block %d (before)\n",hold.non_reserved_inode_no,hold.block_no);
                                            hold.block_no=swap_block;
                                            /*printf("lowest_non_reserved_block is %u, corresponding_inode is %u,block_no is %u\n",lowest_non_reserved_block,corresponding_inode,hold.block_no);
                                
                                            printf("inode %d has block %d (after)\n",hold.non_reserved_inode_no,hold.block_no);*/
                                            *it=hold;
                                            break;
                                            
                                         }
                                    }
                                    v.erase(v.begin()+z);
                                    /*---------------------------update table done-------------------------------------*/
                                    lseek(fd, BLOCK_OFFSET(group.bg_inode_table)+(sizeof(struct ext2_inode) * (corresponding_inode-1)), SEEK_SET); //find whose block is replaced inode.
                                    read(fd,&inode_r,sizeof(struct ext2_inode));
                                    for(int m=0;m<15;m++){
                                        if(inode_r.i_block[m]==lowest_non_reserved_block){//find the previous pointer
                                            inode_r.i_block[m] = swap_block; //update bp inode.
                                            break;
                                        }
                                    }
                                    lseek(fd,sizeof(struct ext2_inode) * -1,SEEK_CUR);//return to the beginning of block after read.
                                    write(fd,&inode_r,sizeof(struct ext2_inode));
                                    //update vector that holds information about non-reserved inode&block pairs.
                                    BM_SET(lowest_non_reserved_block-1,bitmap);//new block is now used, normally already being used blocks would be swapped.     
                                }
                            }
                        }
                        //Now update the 12th block in the inode.
                        lseek(fd, BLOCK_OFFSET(group.bg_inode_table)+(sizeof(struct ext2_inode) * super.s_inodes_count) + (block_size * (inode.i_block[j]-9)), SEEK_SET); // go to the indirect data pointer block.
                        write(fd,&indirect_data_block,block_size);//place the data block.       
                    }//indirect pointer
                    
                }
            }
        }
    }

    //After defragmentation
    for(int i=super.s_first_ino;i<super.s_inodes_count;i++){
        struct ext2_inode inode;
        lseek(fd, BLOCK_OFFSET(group.bg_inode_table)+(sizeof(struct ext2_inode) * i), SEEK_SET); // +sizeof(struct ext2_inode) *i is done to read all reserved inodes consecutively.
        read(fd, &inode, sizeof(struct ext2_inode));
        if(inode.i_size>0 && BM_ISSET(i,ibitmap)){
            for(int j=0; j < 15; j++){
                if(inode.i_block[j]>0){
                    if(j<12)
                        printf("%d %d\n",i+1,inode.i_block[j]);
                
                    else if(j==12){//indirect pointer
                        printf("%d %d\n",i+1,inode.i_block[j]);
                        unsigned int indirect_data_block[block_size/sizeof(unsigned int)];
                        lseek(fd, BLOCK_OFFSET(group.bg_inode_table)+(sizeof(struct ext2_inode) * super.s_inodes_count) + (block_size * (inode.i_block[j]-9)), SEEK_SET); // go to the indirect data pointer block.
                        read(fd,&indirect_data_block,block_size);
                        for(unsigned int x=0;x<block_size/sizeof(unsigned int);x++){
                            if(indirect_data_block[x]>0){
                                printf("%d %d\n",i+1,indirect_data_block[x]);
                            }
                        }          
                    }
                 }
            }
        }
    }

    free(ibitmap);
    free(bitmap);
    close(fd);
    return 0;
}
