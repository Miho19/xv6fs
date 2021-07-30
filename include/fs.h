#ifndef FS_H_
#define FS_H_

#include <stdio.h>

#define BSIZE 512
#define ROOT_INO 1

#define NDIRECT 12
#define NINDIRECT (BSIZE / sizeof(uint))
#define MAXFILE (NDIRECT + NINDIRECT)

#define T_DIR 1
#define T_FILE 2
#define T_DEV 3

#define WRITE_MODE 1
#define READ_MODE 0

// Inodes per block.
#define IPB           (BSIZE / sizeof(struct dinode))

// Block containing inode i
#define IBLOCK(i)     ((i) / IPB + 2)

// Bitmap bits per block
#define BPB           (BSIZE*8)

// Block containing bit for block b
#define BBLOCK(b, ninodes) (b/BPB + (ninodes)/IPB + 3)

// Directory is a file containing a sequence of dirent structures.
#define DIRSIZ 14

typedef unsigned short ushort;

struct dirent {
  ushort inum;
  char name[DIRSIZ];
};

struct dirent_offset {
  uint sector;
  uint offset;
};

typedef unsigned int uint;

struct superblock {
  uint size;         
  uint nblocks;      
  uint ninodes;      
  uint nlog;         
};


// On-disk inode structure
struct dinode {
  short type;           // File type
  short major;          // Major device number (T_DEV only)
  short minor;          // Minor device number (T_DEV only)
  short nlink;          // Number of links to inode in file system
  uint size;            // Size of file (bytes)
  uint addrs[NDIRECT+1];   // Data block addresses
};







/**
    Interface to reading and writing
        currently set to fs.img.
*/
int rsec(int, void *, FILE *);
int wsec(int, void *, FILE *);


/** Test methods used by init code */
void superblock_init(FILE *f);







/** 

    FILE.h

*/


// in-memory copy of an inode
struct inode {
  uint dev;           // Device number
  uint inum;          // Inode number
  int ref;            // Reference count
  int flags;          // I_BUSY, I_VALID

  short type;         // copy of disk inode
  short major;
  short minor;
  short nlink;
  uint size;
  uint addrs[NDIRECT+1];
};


int iget(uint inum, struct inode *ip, FILE *f);
int iupdate(struct inode *ip, FILE *f);
int ialloc(struct inode *ip, short type, FILE *f);
int ilink(uint parent, const char *name, struct inode *ip, FILE *f);
int iremove(struct inode *ip, FILE *f);

int nparent(uint parent, const char *name, struct inode *ip, FILE *f, struct dirent_offset * doff);
int iparent(uint parent, uint query, char *name, struct inode *ip, FILE *f, struct dirent_offset * doff);

uint bmapw(struct inode *ip, uint bn, FILE *f);
uint bmapr(struct inode *ip, uint bn, FILE *f);


uint blkalloc(FILE *f);

#endif