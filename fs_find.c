#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include<string.h>
#include <malloc_np.h>
#include <fcntl.h>
#include "fs.h"
#include "dinode.h"
#include "dir.h"
#include <sys/mman.h>


void traverse_dir_inode(uint64_t inode_index, char* preface);
uint64_t traverse_dir_data_block(uint64_t block_index, uint64_t di_size,  char* preface);
uint64_t find_inode_index(uint32_t inode_entry);
uint64_t traverse_indirect_block (uint64_t iblock, uint64_t size, char* preface);

//to be filled once super block is found
struct fs* super_block;
int frag_size;// in bytes
int block_size;
int num_cgroup; // number of cylindrical groups
int size_cgroup;
uint64_t* cs_inode_table; // an array that holds the starting location
                         // for the cylindar's inode table

char * buff; //a buffer to store disk raw bits



int main(int argc, char *argv[])
{
    //printf("%s", argv[1]);
    int fp = open(argv[1], O_RDONLY); //open file
    if (fp == -1){
        printf("open error");
    }
    int size = lseek(fp, 0, SEEK_END); // get offset at end of file
    lseek(fp, 0, SEEK_SET); // seek back to beginning
    buff = mmap(NULL, size, PROT_READ, MAP_SHARED, fp, 0);// allocate enough memory.
    if (buff == MAP_FAILED){
        printf("Mapping Failed\n");
        return 1;
    }
	close(fp);



    /*
    From fs.h:

        * Depending on the architecture and the media, the superblock may
        * reside in any one of four places. For tiny media where every block
        * counts, it is placed at the very front of the partition. Historically,
        * UFS1 placed it 8K from the front to leave room for the disk label and
        * a small bootstrap. For UFS2 it got moved to 64K from the front to leave
        * room for the disk label and a bigger bootstrap, and for really piggy
        * systems we check at 256K from the front if the first three fail. In
        * all cases the size of the superblock will be SBLOCKSIZE. All values are
        * given in byte-offset form, so they do not imply a sector size. The
        * SBLOCKSEARCH specifies the order in which the locations should be searched.
    We used fsdump ada1p1 to read the super block info for this partition. We
    found that the magic number is 19540119 (424935705 in decimal). We casted
    location 0 and 8k to struct fs but none of them generated the same magic
    number. When casting the 64k element of the .img file into a struct fs, it
    had the same magic number we are looking for. Thus, the super block for
    ada1p1 starts at 64k.
    */
    super_block = (struct fs*)&buff[65536];
    frag_size = super_block -> fs_fsize;// in bytes
    block_size = super_block ->fs_bsize;
    num_cgroup = super_block -> fs_ncg; // number of cylindrical groups
    size_cgroup = super_block -> fs_cgsize; //size of cylindrical group

    /*an array that holds the starting location for the cylindar's inode table*/
    uint64_t temp [num_cgroup];
    cs_inode_table = temp;
    for (int i = 0; i< num_cgroup; i++){
    	cs_inode_table[i] = cgimin(super_block, i)*frag_size;
    }


	// printf("block size: %d\n", block_size);
    // printf("frag size: %d\n", frag_size);
    // printf("inode size: %d\n", 256);
    // printf("super block: %d\n", 65536);
	/*for this test file, the number of cylinders is 4. The parent directory is located in inode 2 of c group 0.
	// from dinode.h:

		 * The root inode is the root of the filesystem.  Inode 0 can't be used for
		 * normal purposes and historically bad blocks were linked to inode 1, thus
		 * the root inode is 2.  (Inode 1 is no longer used for this purpose, however
		 * numerous dump tapes make this assumption, so we are stuck with it).
	*/
	int c0_inode_table = cgimin(super_block, 0)*frag_size;
	int root_inode_loc = cs_inode_table[0] + 2*256;

    traverse_dir_inode(root_inode_loc, "");
}

// index in bytes
void traverse_dir_inode(uint64_t inode_index, char* preface){
    struct ufs2_dinode* inode = (struct ufs2_dinode*)&buff[inode_index];
    uint64_t size = inode->di_size;
    for (int i = 0; i < UFS_NDADDR; i++){
        if (inode->di_db[i] == 0){
            break;
        }
        uint64_t block_index = inode->di_db[i]*frag_size;
        traverse_dir_data_block(block_index, size, preface);

    }

    if (inode->di_ib[0] != 0){
        size = traverse_indirect_block(inode->di_ib[0], size, preface);
    }


}





uint64_t traverse_indirect_block (uint64_t iblock, uint64_t size, char* preface){
    uint64_t iblock_index = iblock*frag_size;
    uint64_t loc_block_index = 0;
    ufs2_daddr_t block_index;


    while ( size != 0 && loc_block_index != block_size) {
        block_index = *(ufs2_daddr_t *)(buff + iblock_index + loc_block_index);//*((ufs2_daddr_t*) &buff[iblock_index + loc_block_index]);
        loc_block_index += sizeof(ufs2_daddr_t);
        size = traverse_dir_data_block(block_index*frag_size, size, preface);
    }

    return size;
}


//index in bytes
uint64_t traverse_dir_data_block(uint64_t block_index, uint64_t di_size, char* preface){
    struct direct* dir = (struct direct *)&buff[block_index];
    char* temp = malloc(strlen(preface) +  UFS_MAXNAMLEN);

    while(dir->d_ino != 0 && di_size != 0){
        if (strcmp(".",  dir->d_name)) {
            if (strcmp("..",  dir->d_name)){
                strcpy(temp, preface);
                strcat(temp, dir->d_name);
                printf("%s\n", temp);
                if (dir->d_type == DT_DIR){
                    char* temp2 = malloc(strlen(preface) + 5);
                    strcpy(temp2, preface);
                    strcat(temp2, "   ");
                    //find inode index and pass it to traverse_dir_inode
                    uint64_t inode_index = find_inode_index(dir->d_ino);
                    traverse_dir_inode(inode_index, temp2);

                    free(temp2);
                }
            }
        }

        block_index = block_index + dir->d_reclen;
        di_size -= dir->d_reclen;
        dir = ( struct direct *)&buff[block_index];
    }
    free(temp);
    return di_size;
}

uint64_t find_inode_index(uint32_t inode_entry){

    uint64_t inode_index = 0;
    uint64_t c_index;
    struct cg* c;
    for (int i = 0; i<num_cgroup; i++){
        c_index = cgtod(super_block, i)*frag_size;
        c =  (struct cg*)&buff[c_index];
        if (inode_entry >= c->cg_niblk){
            inode_entry -= c->cg_niblk;
        }else{
            inode_index = cs_inode_table[i] + inode_entry*256;
            break;
        }
    }

	return inode_index;

}
