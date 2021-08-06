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

#define MIN(x, y) ( (x) < (y) ? (x) : (y))

#define DEBUG 0

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


typedef struct io_ops {
  int(*rsec)(int, void *);
  int(*wsec)(int, void *);
  FILE *f;
} IO;

extern struct io_ops io;

int rsec(int, void *);
int wsec(int, void *);


void superblock_read();
void readsb(struct superblock *sb);

int iget(uint inum, struct inode *ip);
int iupdate(struct inode *ip);
int ialloc(struct inode *ip, short type);
int ilink(uint parent, const char *name, struct inode *ip);
int iremove(struct inode *ip);

int iread(struct inode *ip, unsigned char *buf, uint n, uint offset);
int iwrite(struct inode *ip, const unsigned char *buf, uint n, uint offset);

int nparent(uint parent, const char *name, struct inode *ip, struct dirent_offset * doff);
int iparent(uint parent, uint query, char *name, struct inode *ip, struct dirent_offset * doff);

int dirinit(uint parent, struct inode *dir);
int iunlink(struct inode *pip, struct inode *ip, struct dirent_offset * doff);
int dirremove(struct inode *pip);

uint bmapw(struct inode *ip, uint bn);
uint bmapr(struct inode *ip, uint bn);


uint blkalloc();

#endif