/*
 When one of these does not hold, print the error message (also shown below) and exit(1) immediately.
 */

/* Block 0 is unused.
 Block 1 is super block.
 Inodes start at block 2.*/

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <string.h>


#define DIRSIZ 14
#define NDIRECT 12

/* File system super block*/
struct superblock {
  uint size;         /* Size of file system image (blocks)*/
  uint nblocks;
  uint ninodes;      /* Number of inodes.*/
};

struct dirent{
  ushort inum;
  char name[DIRSIZ];
};

/* On-disk inode structure*/
struct dinode { /* total size 64 bytes *8 per block 0th unused*/
  short type;           /* File type 0 unused 1 directory 2 regular file*/
  short major;          /* Major device number (T_DEV only) X*/
  short minor;          /* Minor device number (T_DEV only) X*/
  short nlink;          /* Number of links to inode in file system need to know hard link(>= 2)*/
  uint size;            /* Size of file (bytes)*/
  uint addrs[NDIRECT+1];   /* Data block addresses +1 indirect entry*/
};


#define ROOTINO 1  /* root i-number*/
#define BSIZE 512  /* block size first unused*/

#define NINDIRECT (BSIZE / sizeof(uint))
#define MAXFILE (NDIRECT + NINDIRECT)
#define IPB (BSIZE / sizeof(struct dinode)) /* Inodes per block.*/
#define IBLOCK(i)     ((i) / IPB + 2) /* Block containing inode i*/
#define BPB (BSIZE*8) /* Bitmap bits per block*/

/* Block containing bit for block b*/
#define BBLOCK(b, ninodes) (b/BPB + (ninodes)/IPB + 3)

/* Directory is a file containing a sequence of dirent structures.*/


char* bitmap;
void* img_ptr;
#define NADDRESS 128
int BLOCKS;
int INODES;
int SIZE;
int* used_Block;
int* used_Inode;
int* inode_link;
int DATA_BEGIN;
struct dinode* INODE_BEGIN;

#define UNUSED 0x0
#define T_DIR 0x1
#define T_FILE 0x2
#define T_DEV 0x3

void* get_block_address(uint block_index, void* imgPtr) {
    return (imgPtr + block_index * BSIZE);
}

struct dinode* get_inode_address(uint node_index) {
    return &INODE_BEGIN[node_index-1];
}

bool cheackValid(int index){ /*block is used or not*/

  //fprintf(stderr, "index %p, bitmap :%d.\n", index, *(int*)bitmap);

  int position = index/32;
  int offset = index%32;
  int x = *(int*)(bitmap + position*4);
  int i = 0;
  for(i = 0; i <offset; i++)
    x = x >> 1;

  x = x & 0x0001;
 // fprintf(stderr, "x :%d.\n", x);

    return (x > 0);
}

int check_bitmap() {
    int i;
    for(i = 0; i < BLOCKS; i++) {
        if(cheackValid(i)) {
            if(used_Block[i] <= 0){
                fprintf(stderr, "ERROR: bitmap marks block in use but it is not in use.\n");
                return 1;
            }
        } else {
            if(used_Block[i] > 0){
                fprintf(stderr, "ERROR: address used by inode but marked free in bitmap.\n");
                return 1;
            }
        }
    }
    return 0;
}

int check_inode_link(){
    int i;
//fprintf(stderr,"========check_inode_link===========\n");
    for(i = 1; i < INODES; i++){
//fprintf(stderr,"inode#%d\ttype%d links %d\t inode_link %d\t used %d\n", i, INODE_BEGIN[i-1].type, INODE_BEGIN[i-1].nlink, inode_link[i], used_Inode[i]);
        if(INODE_BEGIN[i-1].type != UNUSED && used_Inode[i] == 0){
            fprintf(stderr, "ERROR: inode marked use but not found in a directory.\n");
            return 1;
        }
        if(INODE_BEGIN[i-1].type == UNUSED && used_Inode[i] != 0){
            fprintf(stderr, "ERROR: inode referred to in directory but marked free.\n");
            return 1;
        }
        if(INODE_BEGIN[i-1].type == T_DIR && inode_link[i] != 2){
            if(i == ROOTINO && inode_link[i] != 1) {
                fprintf(stderr, "ERROR: directory appears more than once in file system.\n");
                return 1;
            }else if (i != ROOTINO){
                fprintf(stderr, "ERROR: directory appears more than once in file system.\n");
                return 1;
            }
        }
        if(INODE_BEGIN[i-1].nlink != inode_link[i]){
            if((i != ROOTINO && INODE_BEGIN[i-1].type == T_DIR) && INODE_BEGIN[i-1].nlink != (inode_link[i] - 1)) {
                fprintf(stderr, "ERROR: bad reference count for file.\n");
                return 1;
            }else if(INODE_BEGIN[i-1].type != T_DIR){
                fprintf(stderr, "ERROR: bad reference count for file.\n");
                return 1;
            }
        }

        
    }
    return 0;
}

int checkRoot(struct dinode inode) {
// fprintf(stderr, "===========in checRoot========type  (%d).====\n", inode.type);
    if(inode.type != T_DIR) {
        fprintf(stderr, "ERROR: root directory does not exist.\n");
        return 1;
    }
    struct dirent* parent = NULL;
    struct dirent* self = NULL;
    int i;
    for(i = 0; i< NDIRECT; i++){
        struct dirent* temp = (struct dirent*)get_block_address(inode.addrs[i],img_ptr);
        int j;
        for(j = 0; j < BSIZE / sizeof(struct dirent); j++) {
            if(strcmp(temp[j].name, ".") == 0) {
                self = &temp[j];
            }else if(strcmp(temp[j].name,"..") == 0) {
                parent = &temp[j];
            }
        }
        if(parent != NULL && self != NULL) {
            break;
        }
    }
    if(parent == NULL || self == NULL) {
        fprintf(stderr, "ERROR: directory not properly formatted.\n");
        return 1;
    }
    if(parent->inum != self->inum) {
        fprintf(stderr, "ERROR: root directory does not exist.\n");
        return 1;
    }
    return 0;
}

int checkMismatch(struct dirent p, uint self) { // for parent
    int i, index_dir;
    struct dinode* parent;
    parent = get_inode_address(p.inum);
    struct dirent* temp;
    int cnt = (parent->size)/ sizeof(struct dirent);
    if(p.inum == self && self != ROOTINO) {
        fprintf(stderr, "ERROR: parent directory mismatch.\n");
        return 1;
    }

//    if(cnt == 0) return 0;

    for(i = 0; i < NDIRECT && cnt > 0; i++) {
        temp = get_block_address(parent->addrs[i], img_ptr);
        for(index_dir = 0; index_dir < BSIZE/sizeof(struct dirent) && cnt > 0; index_dir++, cnt--){
          if(temp[index_dir].inum == self) {
            return 0;
          }
        }
    }
    uint* ptr = (uint*)get_block_address(parent->addrs[NDIRECT], img_ptr);

    for(i = 0; i < NADDRESS && cnt > 0; i++) {
        temp = get_block_address(*(ptr + i), img_ptr);
        for(index_dir = 0; index_dir < BSIZE/sizeof(struct dirent) && cnt > 0; index_dir++, cnt--){
             if(temp[index_dir].inum == self) {
                return 0;
             }
        }
    }
    fprintf(stderr, "ERROR: parent directory mismatch.\n");
    return 1;
}

int checkEntry(struct dinode inode, uint index_n) { /* for T_DIR*/
//printf("=========in checkEntry==============\n");
    struct dirent* parent = NULL;
    struct dirent* self = NULL;
    int cnt = (inode.size)/ sizeof(struct dirent);
//printf("\n\ninode%d size is %d", index_n, inode.size);
    if(cnt == 0) {
        fprintf(stderr, "ERROR: directory not properly formatted.\n");
        return 1;
    }
    int i;
    for(i = 0; i < NDIRECT; i++){
        struct dirent* temp = (struct dirent*)get_block_address(inode.addrs[i],img_ptr);
        int j;
        for(j = 0; j < BSIZE / sizeof(struct dirent) && cnt >= 0; j++, cnt--) {
//printf("\ni = %d j = %d addr = %d name = %s \n", i, j, temp[j].inum, temp[j].name);
            if(strcmp(temp[j].name, ".") == 0) {
                self = &temp[j];
            }else if(strcmp(temp[j].name,"..") == 0) {
                parent = &temp[j];
            }
        }
        if(parent != NULL && self != NULL) {
            break;
        }
    }
    if(parent == NULL || self == NULL) {
        fprintf(stderr, "ERROR: directory not properly formatted.\n");
        return 1;
    }
    /*
    if(self->inum != index_n) {
        fprintf(stderr, "ERROR: directory not properly formatted.\n");
        return 1;
    }
    */
    if(checkMismatch(*parent, (uint)(self->inum)) == 1){
        return 1;
    }
    return 0;
}

void access_file(struct dinode inode) {
//printf("=========in access==============\n");
    int i, j, index_dir;
    struct dirent* temp;
    int cnt = (inode.size)/ sizeof(struct dirent);

    for(i = 0; i < NDIRECT && cnt > 0; i++) {
        temp = get_block_address(inode.addrs[i], img_ptr);
        for(index_dir = 0; index_dir < BSIZE/sizeof(struct dirent) && cnt > 0; index_dir++, cnt--){
            if(temp[index_dir].inum == 0){
                continue;
            }
            if(strcmp(temp[index_dir].name,"..") == 0) {
//printf("\n==only used==i = %d j = %d addr = %d name = %s \n", i, index_dir, temp[index_dir].inum, temp[index_dir].name);
                used_Inode[temp[index_dir].inum]++;
            } else {
//printf("\ni = %d j = %d addr = %d name = %s \n", i, index_dir, temp[index_dir].inum, temp[index_dir].name);
                inode_link[temp[index_dir].inum]++;
                used_Inode[temp[index_dir].inum]++;
            }
        }
    }
    uint* ptr = (uint*)get_block_address(inode.addrs[NDIRECT], img_ptr);

    for(i = 0; i < NADDRESS && cnt > 0; i++) {
        temp = get_block_address(*(ptr + i), img_ptr);
        for(index_dir = 0; index_dir < BSIZE/sizeof(struct dirent) && cnt > 0; index_dir++, cnt--){
            if(temp[i].inum == 0) {
                continue;
            }
            if(strcmp(temp[index_dir].name,"..") == 0) {
//printf("\n==only used==i = %d j = %d addr = %d name = %s \n", i, index_dir, temp[index_dir].inum, temp[index_dir].name);
                used_Inode[temp[index_dir].inum]++;
            } else {
//printf("\ni = %d j = %d addr = %d name = %s \n", i, index_dir, temp[index_dir].inum, temp[index_dir].name);
                inode_link[temp[index_dir].inum]++;
                used_Inode[temp[index_dir].inum]++;
            }
        }
    }
}

int checkInode(struct dinode inode, uint index_b, uint index_n) {
 //fprintf(stderr, "====checkInode #%d  (%d) links%d.=======\n", index_n, inode.type, inode.nlink);
    if(index_n == 1){ /* todo index != 1 prarent num = self num */
        if(checkRoot(inode) == 1){
            return 1;
        }
    }
    if (inode.type != T_FILE && inode.type != T_DIR && inode.type != T_DEV && inode.type != UNUSED) {
        fprintf(stderr, "ERROR: bad inode.\n");
        return 1;
    }

    int cnt = inode.size;
    if(inode.type == UNUSED) {
        return 0;
    } else if(!cheackValid(index_b)) {
        fprintf(stderr, "ERROR: address used by inode but marked free in bitmap.\n");
        return 1;
    }
    int i;
/*===================address valid or not===========================*/
    for(i = 0; i < NDIRECT && cnt >= 0; i++) {
        if(inode.addrs[i] <= 0) break;
        if(cnt == 0) break;
        if(inode.addrs[i] > BLOCKS || 0 > inode.addrs[i]) {
            fprintf(stderr, "ERROR: bad direct address in inode.\n");
            return 1;
        }
        if(cheackValid(inode.addrs[i])){
            if(DATA_BEGIN > inode.addrs[i]) {
                fprintf(stderr, "ERROR: bad direct address in inode.\n");
                return 1;
            }
            if(used_Block[inode.addrs[i]] > 0) {
                fprintf(stderr, "ERROR: direct address used more than once.\n");
                return 1;
            }
        } else {
            fprintf(stderr, "ERROR: address used by inode but marked free in bitmap.\n");
            return 1;
        }
        cnt = cnt - BSIZE;
//fprintf(stderr, "\nblock#%d\t",inode.addrs[i]);
        used_Block[inode.addrs[i]]++;
    }
    if(cnt > 0 ){
        if(inode.addrs[NDIRECT] > BLOCKS || 0 > inode.addrs[NDIRECT]) {
            fprintf(stderr, "ERROR: bad indirect address in inode.\n");
            return 1;
        }
        if(cnt > 0 && !cheackValid(inode.addrs[NDIRECT])) {
              fprintf(stderr, "ERROR: address used by inode but marked free in bitmap.\n");
              return 1;
        }
        if(cheackValid(inode.addrs[NDIRECT]) && (DATA_BEGIN > inode.addrs[NDIRECT])) {
          fprintf(stderr, "ERROR: bad indirect address in inode.\n");
          return 1;
        }
        if(used_Block[inode.addrs[NDIRECT]] != 0) {
            fprintf(stderr, "ERROR: indirect address used more than once.\n");
            return 1;
        }
//fprintf(stderr, "\nblock#%d\t",inode.addrs[NDIRECT]);
        used_Block[inode.addrs[NDIRECT]]++;
        uint* ptr = (uint*)get_block_address(inode.addrs[NDIRECT], img_ptr);
        for(i = 0; i < NADDRESS && cnt > 0; i++) {
           if(*(ptr + i) == 0) break;
           if(*(ptr + i) > BLOCKS || 0 > *(ptr + i)) {
               fprintf(stderr, "ERROR: bad indirect address in inode.\n");
               return 1;
           }
           if(cheackValid(*(ptr + i))) {
               if(*(ptr + i) > BLOCKS || DATA_BEGIN > *(ptr + i)) {
                    fprintf(stderr, "ERROR: bad indirect address in inode.\n");
                    return 1;
               }
           }else {
               fprintf(stderr, "ERROR: address used by inode but marked free in bitmap.\n");
               return 1;
           }
           if(used_Block[*(ptr + i)] != 0) {
               fprintf(stderr, "ERROR: indirect address used more than once.\n");
               return 1;
           }
           cnt = cnt - BSIZE;
//fprintf(stderr, "\nblock#%d\t",*(ptr + i));
           used_Block[*(ptr + i)]++;
         }
     }
    /*===================over address valid or not===========================*/


   // fprintf(stderr, "finish used_Block...\n");

    if(inode.type == T_DIR) {
        if(checkEntry(inode, index_n) == 1)
            return 1;
/*
        if(inode.nlink != 1) {
            fprintf(stderr, "ERROR: directory appears more than once in file system.\n");
            return 1;
        }
*/
        access_file(inode);
//        used_Inode[index_n]++;
    } else if(inode.type == T_FILE) {
//        used_Inode[index_n]++;
    }
    return 0;
}

int main(int argc, char *argv[]) {
  // fprintf(stderr, "========\n");
    if(argc != 2){
        fprintf(stderr, "Usage: xv6_fsck file_system_image\n");
        exit(1);
      }

// fprintf(stderr, "========%s\n", argv[1]);

    int fd = open (argv[1], O_RDONLY);//argv[1]
    if(fd < 0 ){
        fprintf(stderr, "image not found.\n");
        exit(1);
    }
    struct stat fbuf;
    if(fstat(fd, &fbuf) != 0 ){
        fprintf(stderr, "fstat error\n");
    }

    img_ptr = (void*)mmap(NULL, fbuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    struct superblock test = *(struct superblock*)(img_ptr + BSIZE) ;
    // fprintf(stderr, "=====================%p===%d==%d\n", img_ptr, *(int*)(img_ptr + 516), test.size);


    uint block_index = 1;
    struct superblock * sb = (struct superblock*) get_block_address(block_index, img_ptr);
    // fprintf(stderr, "==========================\n");

    BLOCKS = sb->nblocks;
    INODES = sb->ninodes;
    SIZE = sb->size;

    /* fprintf(stderr, "BLOCKS %d\n", BLOCKS);
    fprintf(stderr, "INODES %d\n", INODES);
    fprintf(stderr, "SIZE %d\n", SIZE); */

    used_Block = (uint*)malloc(sizeof(uint) * BLOCKS);
    used_Inode = (uint*)malloc(sizeof(uint) * INODES + 1);
    inode_link = (uint*)malloc(sizeof(uint) * INODES + 1);
    DATA_BEGIN = INODES / IPB + 4;


    int i;
    for(i = 0; i < BLOCKS; i++){
        used_Block[i] = 0;
    }
    for(i = 0; i < INODES + 1; i++){
        used_Inode[i] = 0;
        inode_link[i] = 0;
    }
    used_Block[0]++;
    used_Block[block_index]++;
    block_index = block_index +1;

    struct dinode* inode;
/*    strncpy(bitmap, (const char *)get_block_address(block_index + INODES, img_ptr), BSIZE);*/
    bitmap = (char*)get_block_address(block_index + INODES/IPB + 1, img_ptr);//INODES
    used_Block[block_index + INODES/IPB + 1]++;
    used_Block[block_index + INODES/IPB]++;

// fprintf(stderr, "======img_ptr %x==inode blocks % INODES/IPB %d ==bitmap %x=====block_index:%d===========\n",
//    img_ptr,  INODES/IPB, (block_index + INODES/IPB +  1) * BSIZE, block_index);


    INODE_BEGIN = ((struct dinode*)get_block_address(block_index, img_ptr)) + i%IPB;
    int j = 0;
    for(i = 0; i < INODES; i++){
        inode = ((struct dinode*)get_block_address(block_index, img_ptr)) + i%IPB;
        if(checkInode(*inode, block_index, i) == 1) {
            free(used_Block);
            free(used_Inode);
            free(inode_link);
            exit(1);
        }
        used_Block[block_index]++;
        if(i/IPB > j) {
            block_index = block_index +1;
            j++;
        }
    }
    if(check_bitmap() == 1) {
            free(used_Block);
            free(used_Inode);
            free(inode_link);
            exit(1);
    }
    if(check_inode_link() == 1) {
            free(used_Block);
            free(used_Inode);
            free(inode_link);
            exit(1);
    }
    free(used_Block);
    free(used_Inode);
    free(inode_link);
    exit(0);
}

