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

void traverse_dir_inode(uint64_t inode_index);
void traverse_dir_data_block(uint64_t block_index, uint64_t di_size);
void traverse_file_inode(uint64_t inode_index);
uint64_t print_file_data_block(uint64_t block_index, uint64_t di_size);
uint64_t find_inode_index(uint32_t inode_entry);
void fill_tokens(char* path);
uint64_t traverse_indirect_block (uint64_t iblock, uint64_t size);
uint64_t find_block_index(uint32_t block_entry);
uint64_t traverse_double_indirect_block (uint64_t iblock, uint64_t size);
uint64_t traverse_triple_indirect_block (uint64_t iblock, uint64_t size);



//to be filled once super block is found
struct fs* super_block;
int frag_size;// in bytes
int block_size;
int num_cgroup; // number of cylindrical groups
int size_cgroup;
uint64_t* cs_inode_table; // an array that holds the starting location
// for the cylindar's inode table

char * buff; //a buffer to store disk raw bits

char** tokens;
int tokens_index;
int num_tokens; // number of dirs in the path + 1 (file name).
                           //num_tokens - 1 is the file name index



int main(int argc, char *argv[])
{

    tokens_index = 0;
    num_tokens = 0;
    fill_tokens(argv[2]);

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

    //printf("%d\n", cgdata(super_block, 0));

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
    traverse_dir_inode(find_inode_index(2));

    free(tokens);
}


void fill_tokens(char* path){
	for (int i = 0; i < strlen(path); i++){
        if (path[i] == '/'){
            num_tokens++;
        }
    }
    if (path[0] != '/'){
        num_tokens++;
    }
    if (path[strlen(path)-1] == '/'){
        num_tokens--;
    }
    if(num_tokens == 0){
        num_tokens = 1;
    }
    int index = 0;
    tokens = malloc(num_tokens * sizeof(char*));
    char* token = strtok(path, "/");
    while (token != NULL) {
        tokens[index] = token;
        index++;
        token = strtok(NULL, "/");
    }

}


// index in bytes
void traverse_dir_inode(uint64_t inode_index){
    struct ufs2_dinode* inode = (struct ufs2_dinode*)&buff[inode_index];
    for (int i = 0; i < UFS_NDADDR; i++){
        if (inode->di_db[i] == 0){
            break;
        }
        uint64_t block_index = inode->di_db[i]*frag_size;
        traverse_dir_data_block(block_index, inode->di_size);
    }
}

//index in bytes
void traverse_dir_data_block(uint64_t block_index, uint64_t di_size){
    struct direct* dir = (struct direct *)&buff[block_index];
    int found = 0;

    while(dir->d_ino != 0){
        //ignore "." dir
        if (strcmp(".",  dir->d_name)) {
            //  ignore ".." dir
            if (strcmp("..",  dir->d_name)){
                // we reached the file name in the search-so search for a file
                if (tokens_index == num_tokens-1){
                    if(dir->d_type != DT_DIR && !strcmp(dir->d_name, tokens[tokens_index])){
                        traverse_file_inode( find_inode_index(dir->d_ino));
                        found = 1;
                        break;
                    }
                }
                if (dir->d_type == DT_DIR && !strcmp(dir->d_name, tokens[tokens_index])){
                    tokens_index++;
                    found = 1;
                    //find current dir inode index and pass it to traverse_dir_inode
                    uint64_t inode_index = find_inode_index(dir->d_ino);
                    traverse_dir_inode(inode_index);
                    break;
                }
            }
        }
        block_index = block_index + dir->d_reclen;
        dir = ( struct direct *)&buff[block_index];
    }
    if (found == 0){
        printf("No such file exists\n");
    }
}



void traverse_file_inode(uint64_t inode_index){
    struct ufs2_dinode* inode = (struct ufs2_dinode*)&buff[inode_index];
    uint64_t size = inode->di_size;
    for (int i = 0; i < UFS_NDADDR; i++){
        if (inode->di_db[i] == 0){
            break;
        }
        uint64_t block_index = inode->di_db[i]*frag_size;
        size = print_file_data_block(block_index, size);

    }
    if (inode->di_ib[0] != 0){
        size = traverse_indirect_block(inode->di_ib[0], size);
    }



    if (inode->di_ib[1] != 0){
        size = traverse_double_indirect_block(inode->di_ib[1], size);
    }

    if (inode->di_ib[2] != 0){
        size = traverse_triple_indirect_block(inode->di_ib[2], size);
    }


}

uint64_t traverse_indirect_block (uint64_t iblock, uint64_t size){
    uint64_t iblock_index = iblock*frag_size;
    uint64_t loc_block_index = 0;
    ufs2_daddr_t block_index;


    while ( size != 0 && loc_block_index != block_size) {
        block_index = *(ufs2_daddr_t *)(buff + iblock_index + loc_block_index);//*((ufs2_daddr_t*) &buff[iblock_index + loc_block_index]);
        loc_block_index += sizeof(ufs2_daddr_t);
        size = print_file_data_block(block_index*frag_size, size);
    }

    return size;
}

uint64_t traverse_double_indirect_block (uint64_t iblock, uint64_t size){
    uint64_t iblock_index = iblock*frag_size;
    uint64_t loc_block_index = 0;
    ufs2_daddr_t block_index;
    while (size != 0 && loc_block_index != block_size) {
        block_index = *(ufs2_daddr_t *)(buff + iblock_index + loc_block_index);
        loc_block_index += sizeof(ufs2_daddr_t);
        size = traverse_indirect_block(block_index, size);
    }

    return size;
}

uint64_t traverse_triple_indirect_block (uint64_t iblock, uint64_t size){
    uint64_t iblock_index = iblock*frag_size;
    uint64_t loc_block_index = 0;
    ufs2_daddr_t block_index;

    while (size != 0 && loc_block_index != block_size) {
        block_index = *(ufs2_daddr_t *)(buff + iblock_index + loc_block_index);
        loc_block_index += sizeof(ufs2_daddr_t);
        size = traverse_double_indirect_block(block_index, size);
    }

    return size;
}

uint64_t print_file_data_block(uint64_t block_index, uint64_t di_size){
    if (di_size <= block_size){
        write(1, &buff[block_index], di_size);
        di_size = 0;
    }else{
        write(1, &buff[block_index], block_size);
        di_size -= block_size;
    }

    return di_size;
}

uint64_t find_inode_index(uint32_t inode_entry){

    uint64_t inode_index = 0;
    uint64_t c_index; // cylindar index
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


uint64_t find_block_index(uint32_t block_entry){

    uint64_t block_index = 0;
    uint64_t c_index= dtog(super_block, block_entry); // cylindar index
    block_index = (cgdmin(super_block, c_index) + dtogd(super_block, block_entry))*frag_size;

    return block_index;

}
