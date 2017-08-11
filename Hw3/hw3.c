#include <fcntl.h>
#include <stdio.h>
#include <cstdlib>
#include <unistd.h>
#include "ext2.h"
#include <vector>
#include <climits>
#include <algorithm>
#define BASE_OFFSET 1024
#define EXT2_BLOCK_SIZE 1024


typedef unsigned char bmap;
#define __NBITS (8 * (int) sizeof (bmap))
#define __BMELT(d) ((d) / __NBITS)
#define __BMMASK(d) ((bmap) 1 << ((d) % __NBITS))
#define BM_SET(d, set) ((set[__BMELT (d)] |= __BMMASK (d)))
#define BM_CLR(d, set) ((set[__BMELT (d)] &= ~__BMMASK (d)))
#define BM_ISSET(d, set) ((set[__BMELT (d)] & __BMMASK (d)) != 0)

int block_size = 0;
#define BLOCK_OFFSET(block) (BASE_OFFSET + (block-1)*block_size)

struct inode_and_block{
    unsigned int non_reserved_inode_no;
    unsigned int block_no;
};
void init_defragmentation(struct ext2_super_block super, struct ext2_group_desc group,int fd, bmap *bitmap, bmap *ibitmap,std::vector<inode_and_block> v);

void u(auto& v,int b){
	v.erase(std::remove_if(v.begin(),v.end(),[&](auto &p){ return p.block_no==b;}),v.end());}
	
int main(int argc,char* argv[])
{
    struct ext2_super_block super;
    struct ext2_group_desc group;
    int fd;
    if(argc < 2)
    {
        exit(1);
    }
    if ((fd = open(argv[1], O_RDWR)) < 0) {
        perror(argv[1]);
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
                        printf("%d %d\n",i+1,inode.i_block[j]);
                        
                        if(j>=12){//indirect pointer
                            unsigned int indirect_data_block[block_size/sizeof(unsigned int)];
                            lseek(fd, BLOCK_OFFSET(inode.i_block[j]), SEEK_SET); // go to the indirect data pointer block.
                            read(fd,&indirect_data_block,block_size);
                            for(unsigned int x=0;x<block_size/sizeof(unsigned int);x++){
                                if(indirect_data_block[x]>0){
                                    struct inode_and_block ind_iab;
                                    ind_iab.non_reserved_inode_no = i+1;
                                    ind_iab.block_no = indirect_data_block[x];
                                    v.push_back(ind_iab);
                                    printf("%d %d\n",i+1,indirect_data_block[x]);
                                    if(j>=13){
                                        unsigned int d_indirect_data_block[block_size/sizeof(unsigned int)];
                                        lseek(fd, BLOCK_OFFSET(indirect_data_block[x]), SEEK_SET); // go to the indirect data pointer block.
                                        read(fd,&d_indirect_data_block,block_size);
                                        for(unsigned int y=0;y<block_size/sizeof(unsigned int);y++){
                                            if(d_indirect_data_block[y]>0){
                                               struct inode_and_block dind_iab;
                                               dind_iab.non_reserved_inode_no = i+1;
                                               dind_iab.block_no = d_indirect_data_block[y];
                                               v.push_back(dind_iab); 
                                               printf("%d %d\n",i+1,d_indirect_data_block[y]);
                                               if(j>=14){
                                                    unsigned int t_indirect_data_block[block_size/sizeof(unsigned int)];
                                                    lseek(fd, BLOCK_OFFSET(d_indirect_data_block[y]), SEEK_SET); // go to the indirect data pointer block.
                                                    read(fd,&t_indirect_data_block,block_size);
                                                    for(unsigned int w=0;w<block_size/sizeof(unsigned int);w++){
                                                        if(t_indirect_data_block[w]>0){
                                                            struct inode_and_block tind_iab;
                                                            tind_iab.non_reserved_inode_no = i+1;
                                                            tind_iab.block_no = t_indirect_data_block[w];
                                                            v.push_back(tind_iab);
                                                            printf("%d %d\n",i+1,t_indirect_data_block[w]);
                                                        }
                                                    }
                                               } 
                                            }
                                        }
                                    }//end if j>=13  
                                }
                            }
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
    init_defragmentation(super,group,fd,bitmap,ibitmap,v);
    
   

    //After defragmentation
    for(int i=super.s_first_ino;i<super.s_inodes_count;i++){
        struct ext2_inode inode;
        lseek(fd, BLOCK_OFFSET(group.bg_inode_table)+(sizeof(struct ext2_inode) * i), SEEK_SET); // +sizeof(struct ext2_inode) *i is done to read all reserved inodes consecutively.
        read(fd, &inode, sizeof(struct ext2_inode));
        if(inode.i_size>0 && BM_ISSET(i,ibitmap)){
            for(int j=0; j < 15; j++){
                if(inode.i_block[j]>0){
                    printf("%d %d\n",i+1,inode.i_block[j]);
                    if(j>=12){//indirect pointer
                        unsigned int indirect_data_block[block_size/sizeof(unsigned int)];
                        lseek(fd, BLOCK_OFFSET(inode.i_block[j]), SEEK_SET); // go to the indirect data pointer block.
                        read(fd,&indirect_data_block,block_size);
                        for(unsigned int x=0;x<block_size/sizeof(unsigned int);x++){
                            if(indirect_data_block[x]>0){
                                printf("%d %d\n",i+1,indirect_data_block[x]);
                                if(j>=13){
                                    unsigned int d_indirect_data_block[block_size/sizeof(unsigned int)];
                                    lseek(fd, BLOCK_OFFSET(indirect_data_block[x]), SEEK_SET); // go to the indirect data pointer block.
                                    read(fd,&d_indirect_data_block,block_size);
                                    for(unsigned int y=0;y<block_size/sizeof(unsigned int);y++){
                                        if(d_indirect_data_block[y]>0){
                                           printf("%d %d\n",i+1,d_indirect_data_block[y]);
                                           if(j>=14){
                                                unsigned int t_indirect_data_block[block_size/sizeof(unsigned int)];
                                                lseek(fd, BLOCK_OFFSET(d_indirect_data_block[y]), SEEK_SET); // go to the indirect data pointer block.
                                                read(fd,&t_indirect_data_block,block_size);
                                                for(unsigned int w=0;w<block_size/sizeof(unsigned int);w++){
                                                    if(t_indirect_data_block[w]>0)
                                                        printf("%d %d\n",i+1,t_indirect_data_block[w]);
                                                }
                                           } 
                                        }
                                    }
                                }//end if j>=13
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


void init_defragmentation(struct ext2_super_block super, struct ext2_group_desc group,int fd, bmap *bitmap, bmap *ibitmap,std::vector<inode_and_block> v){
     //Init defragmentation
    for(int i=super.s_first_ino;i<super.s_inodes_count;i++){
        struct ext2_inode inode;
        lseek(fd, BLOCK_OFFSET(group.bg_inode_table)+(sizeof(struct ext2_inode) * i), SEEK_SET); 
        read(fd, &inode, sizeof(struct ext2_inode));
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
                    /*---------------------------------------------------------------------*/
                    /*---------------------------------------3-----------------------------*/
                    if(lowest_free_block < lowest_non_reserved_block){
			u(v,inode.i_block[j]);                        
			char data_block[block_size];
                        lseek(fd, BLOCK_OFFSET(inode.i_block[j]), SEEK_SET); // go to the to be placed data block
                        read(fd,&data_block,block_size); //read the to be placed data block.
                        lseek(fd,block_size * -1,SEEK_CUR);//return to the beginning of block after read.
                        write(fd,0,block_size); //zero out the memory.
                        
                        lseek(fd, BLOCK_OFFSET(lowest_free_block), SEEK_SET); // go to the placee data block
                        write(fd,&data_block,block_size);//place the data block.
                        BM_CLR(inode.i_block[j]-1,bitmap);//old block is now free
                        inode.i_block[j]=lowest_free_block;
                        BM_SET(lowest_free_block-1,bitmap);//new block is now used
			
                    }
                    /*---------------------------------------------------------------------*/
                    else{
                        //printf("2-lowest free block is %u, lowest_non_reserved_block is %u\n",lowest_free_block,lowest_non_reserved_block);
                        char data_block_tbp[block_size];//to be placed data block
                        char data_block_bp[block_size];//placee data block.
                        unsigned int swap_block;
                        struct ext2_inode inode_r;
                        lseek(fd, BLOCK_OFFSET(inode.i_block[j]), SEEK_SET); // go to the to be placed data block
                        read(fd,&data_block_tbp,block_size); //read the to be placed data block.

                        lseek(fd, BLOCK_OFFSET(lowest_non_reserved_block), SEEK_SET); // go to the placee data block
                        read(fd,&data_block_bp,block_size);//read the placee data block.
                        lseek(fd,block_size * -1,SEEK_CUR);//return to the beginning of block after read.
                        write(fd,&data_block_tbp,block_size);//write tbp block to the new place.
                        
                        lseek(fd, BLOCK_OFFSET(inode.i_block[j]), SEEK_SET); // go to the to be placed data block
                        write(fd,&data_block_bp,block_size);//write bp block to the old place.

                        swap_block=inode.i_block[j];
                        inode.i_block[j]=lowest_non_reserved_block; //update tbp inode.
                        /* --------------------------update own table---------------------------------------*/
                        unsigned int z=0;
                        for (std::vector<inode_and_block>::iterator it = v.begin() ; it != v.end(); ++it,++z){
                             struct inode_and_block hold = *it;
                             if(hold.non_reserved_inode_no == i+1 && hold.block_no==swap_block)
                             {
                                hold.block_no=lowest_non_reserved_block;
                                *it = hold;
                                break; //update table

                                
                             }          
                        }
                        for (std::vector<inode_and_block>::iterator it = v.begin() ; it != v.end(); ++it){
                             struct inode_and_block hold = *it;
                             if(hold.block_no == lowest_non_reserved_block && hold.non_reserved_inode_no == corresponding_inode){
                                hold.block_no=swap_block;
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

                    

                    if(j>=12){
                        unsigned int indirect_data_block[block_size/sizeof(unsigned int)];
                        lseek(fd, BLOCK_OFFSET(inode.i_block[j]), SEEK_SET); // go to the indirect data pointer block.
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
				    u(v,indirect_data_block[x]); 	
                                    char data_block[block_size];
                                    lseek(fd,BLOCK_OFFSET(indirect_data_block[x]),SEEK_SET);
                                    read(fd,&data_block,block_size); //read the to be placed data block.
                                    lseek(fd,block_size * -1,SEEK_CUR);//return to the beginning of block after read.
                                    write(fd,0,block_size);//zero out the memory.
                                    lseek(fd, BLOCK_OFFSET(lowest_free_block), SEEK_SET); // go to the placee data block
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

                                    //printf("indirect data block [%u] = %u\n",x,indirect_data_block[x]);
                                    lseek(fd, BLOCK_OFFSET(indirect_data_block[x]), SEEK_SET); // go to the to be placed data block
                                    read(fd,&data_block_tbp,block_size); //read the to be placed data block.

                                    lseek(fd, BLOCK_OFFSET(lowest_non_reserved_block), SEEK_SET); // go to the placee data block
                                    read(fd,&data_block_bp,block_size);//read the placee data block.
                                    lseek(fd,block_size * -1,SEEK_CUR);//return to the beginning of block after read.
                                    write(fd,&data_block_tbp,block_size);//write tbp block to the new place.
                                    
                                    lseek(fd, BLOCK_OFFSET(indirect_data_block[x]), SEEK_SET); // go to the to be placed data block
                                    write(fd,&data_block_bp,block_size);//write bp block to the old place.
                                    swap_block=indirect_data_block[x];
                                    indirect_data_block[x]=lowest_non_reserved_block; //update tbp inode.
                                    /* --------------------------update own table---------------------------------------*/
                                    unsigned int z=0;
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
                                if(j>=13){//Double indirect pointer
                                    //printf("In double indirect block, reaching to indirect block %u\n",indirect_data_block[x]);
                                    unsigned int d_indirect_data_block[block_size/sizeof(unsigned int)];
                                    lseek(fd, BLOCK_OFFSET(indirect_data_block[x]), SEEK_SET); // go to the indirect data pointer block.
                                    read(fd,&d_indirect_data_block,block_size);
                                    //struct ext2_inode d_inode_r;
                                    for(unsigned int y=0;y<block_size/sizeof(unsigned int);y++){
                                        if(d_indirect_data_block[y]>0){
                                            unsigned int d_lowest_free_block;
                                            unsigned int d_lowest_non_reserved_block = UINT_MAX;//Max unsigned int
                                            unsigned int d_corresponding_inode = UINT_MAX; 
                                            for (int k = 0; k < super.s_blocks_count; k++){
                                                if (!BM_ISSET(k,bitmap)){
                                                    d_lowest_free_block = k+1;
                                                    break;
                                                }                           
                                            } //lowest free block
                                            for (std::vector<inode_and_block>::iterator it = v.begin() ; it != v.end(); ++it){
                                                 struct inode_and_block hold = *it;
                                                 if(hold.block_no < d_lowest_non_reserved_block)
                                                 {
                                                    d_lowest_non_reserved_block = hold.block_no;
                                                    d_corresponding_inode = hold.non_reserved_inode_no;
                                                 }
                                            }//lowest_non_reserved_block
                                            if(d_lowest_free_block < d_lowest_non_reserved_block){
                                                /*printf("1-lowest free block is %u, lowest_non_reserved_block is %u\n",d_lowest_free_block,d_lowest_non_reserved_block);
                                                printf("Trying to update inode %u\n",i+1);
                                                */
                                                u(v,d_indirect_data_block[y]);
						char data_block[block_size];
                                                lseek(fd,BLOCK_OFFSET(d_indirect_data_block[y]),SEEK_SET);
                                                read(fd,&data_block,block_size); //read the to be placed data block.
                                                lseek(fd,block_size * -1,SEEK_CUR);//return to the beginning of block after read.
                                                write(fd,0,block_size);//zero out the memory.
                                                lseek(fd, BLOCK_OFFSET(d_lowest_free_block), SEEK_SET); // go to the placee data block
                                                write(fd,&data_block,block_size);//place the data block.
                                                BM_CLR(d_indirect_data_block[y]-1,bitmap);//old block is now free
                                                d_indirect_data_block[y]=d_lowest_free_block;
                                                BM_SET(d_lowest_free_block-1,bitmap);//new block is now used
						
                                            }
                                            else{
                                                char data_block_tbp[block_size];//to be placed data block
                                                char data_block_bp[block_size];//placee data block.
                                                unsigned int swap_block;
                                                struct ext2_inode inode_r;

                                                lseek(fd, BLOCK_OFFSET(d_indirect_data_block[y]), SEEK_SET); // go to the to be placed data block
                                                read(fd,&data_block_tbp,block_size); //read the to be placed data block.

                                                lseek(fd, BLOCK_OFFSET(d_lowest_non_reserved_block), SEEK_SET); // go to the placee data block
                                                read(fd,&data_block_bp,block_size);//read the placee data block.
                                                lseek(fd,block_size * -1,SEEK_CUR);//return to the beginning of block after read.
                                                write(fd,&data_block_tbp,block_size);//write tbp block to the new place.
                                                
                                                lseek(fd, BLOCK_OFFSET(d_indirect_data_block[y]), SEEK_SET); // go to the to be placed data block
                                                write(fd,&data_block_bp,block_size);//write bp block to the old place.
                                                swap_block=d_indirect_data_block[y];
                                                d_indirect_data_block[y]=d_lowest_non_reserved_block; //update tbp inode.
                                                /* --------------------------update own table---------------------------------------*/
                                                unsigned int z=0;
                                                for (std::vector<inode_and_block>::iterator it = v.begin() ; it != v.end(); ++it,++z){
                                                     struct inode_and_block hold = *it;
                                                     if(hold.non_reserved_inode_no == i+1 && hold.block_no==swap_block)
                                                     {
                                                        hold.block_no=d_lowest_non_reserved_block;
                                                        *it = hold;
                                                        break; //update table                               
                                                     }          
                                                }
                                                for (std::vector<inode_and_block>::iterator it = v.begin() ; it != v.end(); ++it){
                                                     struct inode_and_block hold = *it;
                                                     if(hold.block_no == d_lowest_non_reserved_block && hold.non_reserved_inode_no == d_corresponding_inode){
                                                        hold.block_no=swap_block;
                                                        *it=hold;
                                                        break;
                                                        
                                                     }
                                                }
                                                v.erase(v.begin()+z);
                                                /*---------------------------update table done-------------------------------------*/
                                                lseek(fd, BLOCK_OFFSET(group.bg_inode_table)+(sizeof(struct ext2_inode) * (d_corresponding_inode-1)), SEEK_SET); //find whose block is replaced inode.
                                                read(fd,&inode_r,sizeof(struct ext2_inode));
                                                for(int m=0;m<15;m++){
                                                    if(inode_r.i_block[m]==d_lowest_non_reserved_block){//find the previous pointer
                                                        inode_r.i_block[m] = swap_block; //update bp inode.
                                                        break;
                                                    }
                                                }
                                                lseek(fd,sizeof(struct ext2_inode) * -1,SEEK_CUR);//return to the beginning of block after read.
                                                write(fd,&inode_r,sizeof(struct ext2_inode));
                                                //update vector that holds information about non-reserved inode&block pairs.
                                                BM_SET(d_lowest_non_reserved_block-1,bitmap);//new block is now used, normally already being used blocks would be swapped.     
                                            }//end else
                                            if(j==14){//triple indirect block
                                                //printf("In triple indirect block, reaching to indirect block %u\n",d_indirect_data_block[y]);
                                                unsigned int t_indirect_data_block[block_size/sizeof(unsigned int)];
                                                lseek(fd, BLOCK_OFFSET(d_indirect_data_block[y]), SEEK_SET); // go to the indirect data pointer block.
                                                read(fd,&t_indirect_data_block,block_size);
                                                for(unsigned int w=0;w<block_size/sizeof(unsigned int);w++){
                                                    if(t_indirect_data_block[w]>0){
                                                        unsigned int t_lowest_free_block;
                                                        unsigned int t_lowest_non_reserved_block = UINT_MAX;//Max unsigned int
                                                        unsigned int t_corresponding_inode = UINT_MAX; 
                                                        for (int k = 0; k < super.s_blocks_count; k++){
                                                            if (!BM_ISSET(k,bitmap)){
                                                                t_lowest_free_block = k+1;
                                                                break;
                                                            }                           
                                                        } //lowest free block
                                                        for (std::vector<inode_and_block>::iterator it = v.begin() ; it != v.end(); ++it){
                                                             struct inode_and_block hold = *it;
                                                             if(hold.block_no < d_lowest_non_reserved_block)
                                                             {
                                                                t_lowest_non_reserved_block = hold.block_no;
                                                                t_corresponding_inode = hold.non_reserved_inode_no;
                                                             }
                                                        }//lowest_non_reserved_block
                                                        if(t_lowest_free_block < t_lowest_non_reserved_block){
                                                            /*printf("1-lowest free block is %u, lowest_non_reserved_block is %u\n",t_lowest_free_block,t_lowest_non_reserved_block);
                                                            printf("Trying to update inode %u\n",i+1);
                                                            */
                                                            u(v,t_indirect_data_block[w]);
							    char data_block[block_size];
                                                            lseek(fd,BLOCK_OFFSET(t_indirect_data_block[w]),SEEK_SET);
                                                            read(fd,&data_block,block_size); //read the to be placed data block.
                                                            lseek(fd,block_size * -1,SEEK_CUR);//return to the beginning of block after read.
                                                            write(fd,0,block_size);//zero out the memory.
                                                            lseek(fd, BLOCK_OFFSET(t_lowest_free_block), SEEK_SET); // go to the placee data block
                                                            write(fd,&data_block,block_size);//place the data block.
                                                            BM_CLR(t_indirect_data_block[w]-1,bitmap);//old block is now free
                                                            t_indirect_data_block[w]=t_lowest_free_block;
                                                            BM_SET(t_lowest_free_block-1,bitmap);//new block is now used
							    
                                                        }
                                                        else{
                                                            char data_block_tbp[block_size];//to be placed data block
                                                            char data_block_bp[block_size];//placee data block.
                                                            unsigned int swap_block;
                                                            struct ext2_inode inode_r;

                                                            lseek(fd, BLOCK_OFFSET(t_indirect_data_block[w]), SEEK_SET); // go to the to be placed data block
                                                            read(fd,&data_block_tbp,block_size); //read the to be placed data block.

                                                            lseek(fd, BLOCK_OFFSET(t_lowest_non_reserved_block), SEEK_SET); // go to the placee data block
                                                            read(fd,&data_block_bp,block_size);//read the placee data block.
                                                            lseek(fd,block_size * -1,SEEK_CUR);//return to the beginning of block after read.
                                                            write(fd,&data_block_tbp,block_size);//write tbp block to the new place.
                                                            
                                                            lseek(fd, BLOCK_OFFSET(t_indirect_data_block[w]), SEEK_SET); // go to the to be placed data block
                                                            write(fd,&data_block_bp,block_size);//write bp block to the old place.
                                                            swap_block=t_indirect_data_block[w];
                                                            t_indirect_data_block[w]=t_lowest_non_reserved_block; //update tbp inode.
                                                            /* --------------------------update own table---------------------------------------*/
                                                            unsigned int z=0;
                                                            for (std::vector<inode_and_block>::iterator it = v.begin() ; it != v.end(); ++it,++z){
                                                                 struct inode_and_block hold = *it;
                                                                 if(hold.non_reserved_inode_no == i+1 && hold.block_no==swap_block)
                                                                 {
                                                                    hold.block_no=t_lowest_non_reserved_block;
                                                                    *it = hold;
                                                                    break; //update table                               
                                                                 }          
                                                            }
                                                            for (std::vector<inode_and_block>::iterator it = v.begin() ; it != v.end(); ++it){
                                                                 struct inode_and_block hold = *it;
                                                                 if(hold.block_no == t_lowest_non_reserved_block && hold.non_reserved_inode_no == t_corresponding_inode){
                                                                    hold.block_no=swap_block;
                                                                    *it=hold;
                                                                    break;
                                                                    
                                                                 }
                                                            }
                                                            v.erase(v.begin()+z);
                                                            /*---------------------------update table done-------------------------------------*/
                                                            lseek(fd, BLOCK_OFFSET(group.bg_inode_table)+(sizeof(struct ext2_inode) * (t_corresponding_inode-1)), SEEK_SET); //find whose block is replaced inode.
                                                            read(fd,&inode_r,sizeof(struct ext2_inode));
                                                            for(int m=0;m<15;m++){
                                                                if(inode_r.i_block[m]==t_lowest_non_reserved_block){//find the previous pointer
                                                                    inode_r.i_block[m] = swap_block; //update bp inode.
                                                                    break;
                                                                }
                                                            }
                                                            lseek(fd,sizeof(struct ext2_inode) * -1,SEEK_CUR);//return to the beginning of block after read.
                                                            write(fd,&inode_r,sizeof(struct ext2_inode));
                                                            //update vector that holds information about non-reserved inode&block pairs.
                                                            BM_SET(t_lowest_non_reserved_block-1,bitmap);//new block is now used, normally already being used blocks would be swapped.     
                                                        }//end else
                                                    }//end if(t_indirect_data_block[w]>0)
                                                }//end for t_indirect
                                            }//end check triple indirect pointer    
                                        }//end if(d_indirect_data_block[y]>0)  
                                    }//end for d_indirect
                                }//end check double indirect pointer
                            }//end if(indirect_data_block[x]>0)
                        }
                        //Now update the 12th block in the inode.
                        lseek(fd, BLOCK_OFFSET(inode.i_block[j]), SEEK_SET); // go to the indirect data pointer block.
                        write(fd,&indirect_data_block,block_size);//place the data block.     
                    }//indirect pointer
                    lseek(fd, BLOCK_OFFSET(group.bg_inode_table)+(sizeof(struct ext2_inode) * i), SEEK_SET); // go back again to inode.
                    write(fd,&inode,sizeof(struct ext2_inode));   
                }
            }
        }
    }
}
