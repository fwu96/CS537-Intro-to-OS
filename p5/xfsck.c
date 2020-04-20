#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>
#define stat xv6_s
#define dirent xv6_d
#include "types.h"
#include "fs.h"
#undef stat
#undef dirent

// same as stat.h
#define T_DIR  1   // Directory
#define T_FILE 2   // File
#define T_DEV  3   // Special device
#define T_INV  0

// glb variables
void *img, *bitmap, *s_blk;
struct superblock *sp;
struct dinode *dip;
uint s_idx;
int *blocks, *inodes, *refs;
char *s_bp;

// Error messages
const char *ERR_SP_BLK = "ERROR: superblock is corrupted.";
const char *ERR_BAD_INODE = "ERROR: bad inode.";
const char *ERR_BAD_DIR = "ERROR: bad direct address in inode.";
const char *ERR_BAD_INDIR = "ERROR: bad indirect address in inode.";
const char *ERR_DIR_FORMATE = "ERROR: directory not properly formatted.";
const char *ERR_ADDR_USE = "ERROR: address used by inode but marked free in bitmap.";
const char *ERR_BMP_USE = "ERROR: bitmap marks block in use but it is not in use.";
const char *ERR_DIR_USE = "ERROR: direct address used more than once.";
const char *ERR_FILE_SIZE = "ERROR: incorrect file size in inode.";
const char *ERR_INODE_NOT_FOUND = "ERROR: inode marked used but not found in a directory.";
const char *ERR_INODE_REF = "ERROR: inode referred to in directory but marked free.";
const char *ERR_BAD_REF = "ERROR: bad reference count for file.";
const char *ERR_DIR_APPEAR = "ERROR: directory appears more than once in file system.";
const char *ERR_USAGE = "Usage: xfsck <file_system_image>";
const char *ERR_IMG_FOUND = "image not found.";
const char *ERR_MMAP = "mmap failed.";
// Error message for extra credits
const char *ERR_DIR_ACCESS = "ERROR: inaccessible directory exists.";
const char *ERR_PAR_MISMATCH = "ERROR: parent directory mismatch.";

void err(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

void filesys_check() {
    // bad inode
    for (int i = 0; i < sp->ninodes; i++) {
        if (dip[i].type < T_INV || dip[i].type > T_DEV) {
            err(ERR_BAD_INODE);
        }
    }
    // direct && indirect address
    for (int i = 0; i < sp->ninodes; i++) {
        if (dip[i].type > T_INV && dip[i].type <= T_DEV) {
            // direct
            for (int j = 0; j < NDIRECT; j++) {
                if (dip[i].addrs[j] != 0 && (dip[i].addrs[j] < s_idx || dip[i].addrs[j] >= s_idx + sp->nblocks)) {
                    err(ERR_BAD_DIR);
                }
                if ((dip[i].addrs[j] != 0) && (blocks[dip[i].addrs[j]] == 1)) {
                    err(ERR_DIR_USE);
                }
                blocks[dip[i].addrs[j]] = 1;
            }
            // indirect
            if ((dip[i].addrs[NDIRECT]) != 0 && (dip[i].addrs[NDIRECT] < s_idx || dip[i].addrs[NDIRECT] >= s_idx + sp->nblocks)) {
                err(ERR_BAD_INDIR);
            }
            blocks[dip[i].addrs[NDIRECT]] = 1;
            uint *indir = (uint *)(img + BSIZE * dip[i].addrs[NDIRECT]);
            for (int j = 0; j < NINDIRECT; j++) {
                if ((indir[j] != 0)&& (indir[j] < s_idx || indir[j] >= s_idx + sp->nblocks)) {
                    err(ERR_BAD_INDIR);
                }
                blocks[indir[j]] = 1;
            }
        }
    }
    // formatted directory
    for (int i = 0; i < sp->ninodes; i++) {
        if (dip[i].type == T_DIR) {
            struct xv6_d *d = (struct xv6_d *)(img + dip[i].addrs[0] * BSIZE);
            if (strcmp(d[1].name, "..") != 0 || strcmp(d[0].name, ".") != 0 || d[0].inum != i) {
                err(ERR_DIR_FORMATE);
            }
        }
    }
    // used address
    for (int i = 0; i < sp->ninodes; i++) {
        if (dip[i].type > T_INV && dip[i].type <= T_DEV) {
            // direct
            for (int j = 0; j < NDIRECT + 1; j++) {
                if (dip[i].addrs[j] == 0) continue;
                uint bp_i = dip[i].addrs[j] / 8;
                uint bp_b = dip[i].addrs[j] % 8;
                if (((s_bp[bp_i] >> bp_b) & 1) == 0) {
                    err(ERR_ADDR_USE);
                }
            }
            // indirect
            uint *indir = (uint *)(img + BSIZE * dip[i].addrs[NDIRECT]);
            for (int j = 0; j < NINDIRECT; j++) {
                uint addr = indir[j];
                if (addr == 0) continue;
                uint bp_i = addr / 8;
                uint bp_b = addr % 8;
                if (((s_bp[bp_i] >> bp_b) & 1) == 0) {
                    err(ERR_ADDR_USE);
                }
            }
        }
    }
    // bitmap mark use
    for (int i = 0; i < sp->nblocks; i++) {
        uint bp_i = (i + s_idx) / 8;
        uint bp_b = (i + s_idx) % 8;
        if (((s_bp[bp_i] >> bp_b) & 1) == 1) {
            if (blocks[i + s_idx] != 1) {
                err(ERR_BMP_USE);
            }
        }
    }
    // tracking inodes used and reference
    for (int i = 0; i < sp->ninodes; i++) {
        if (dip[i].type == T_DIR) {
            for (int j = 0; j < NDIRECT; j++) {
                if (dip[i].addrs[j] == 0) continue;
                struct xv6_d *d = (struct xv6_d *)(img + dip[i].addrs[j] * BSIZE);
                for (int k = 0; k < (BSIZE / sizeof(struct xv6_d)); k++) {
                    if (j > 0 || k > 1) {
                        refs[d[k].inum] += 1;
                    }
                    inodes[d[k].inum] += 1;
                }
            }
            uint *indir = (uint *)(img + BSIZE * dip[i].addrs[NDIRECT]);
            for (int j = 0; j < NINDIRECT; j++) {
                if (indir[j] == 0) continue;
                struct xv6_d *d = (struct xv6_d *)(img + indir[j] * BSIZE);
                for (int k = 0; k < (BSIZE / sizeof(struct xv6_d)); k++) {
                    inodes[d[k].inum] += 1;
                    refs[d[k].inum] += 1;
                }
            }
        }
    }
    // inode not found
    for (int i = 1; i < sp->ninodes; i++) {
        if (dip[i].type > T_INV && dip[i].type <= T_DEV) {
            if (inodes[i] == 0) {
                err(ERR_INODE_NOT_FOUND);
            }
        }
    }
    // inode referred
    for (int i = 1; i < sp->ninodes; i++) {
        if (inodes[i] != 0) {
            if (dip[i].type == T_INV) {
                err(ERR_INODE_REF);
            }
        }
    }
    // reference count
    for (int i = 1; i < sp->ninodes; i++) {
        if (dip[i].type == T_FILE) {
            if (inodes[i] != dip[i].nlink) {
                err(ERR_BAD_REF);
            }
        }
    }
    // dir appears more than once
    for (int i = 1; i < sp->ninodes; i++) {
        if (dip[i].type == T_DIR) {
            if (refs[i] > 1) {
                err(ERR_DIR_APPEAR);
            }
        }
    }
    // incorrect file size
    for (int i = 0; i < sp->ninodes; i++) {
        int cnt = 0;  // count for used blocks
        if (dip[i].type > T_INV && dip[i].type <= T_DEV) {
            for (int j = 0; j < NDIRECT; j++) {
                if (dip[i].addrs[j] != 0) cnt++;
            }
            uint *indir = (uint *)(img + dip[i].addrs[NDIRECT] * BSIZE);
            for (int j = 0; j < NINDIRECT; j++) {
                if (indir[j] != 0) cnt++;
            }
            if (cnt != 0 && (dip[i].size <= (cnt - 1) * BSIZE || dip[i].size > cnt * BSIZE)) {
                err(ERR_FILE_SIZE);
            }
        }
    }
}
void filesys_extra_check() {
    // accessibility
    for (int i = 1; i < sp->ninodes; i++) {
        if (dip[i].type == T_DIR) {
            int flg = 0;
            struct dinode *crr = &dip[i];
            for (int j = 1; j < sp->ninodes + 1; j++) {
                for (int k = 0; k < NDIRECT; k++) {
                    if (crr->addrs[k] == 0) continue;
                    struct xv6_d *d = (struct xv6_d *)(img + crr->addrs[k] * BSIZE);
                    for (int n = 0; n < BSIZE / sizeof(struct xv6_d); n++) {
                        if (!strcmp(d[n].name, "..")) {
                            if (d[n].inum == 1) flg = 1;
                            crr = &dip[d[n].inum];
                            break;
                        }
                    }
                }
                if (flg == 1) break;
                if (j == sp->ninodes) {
                    err(ERR_DIR_ACCESS);
                }
            }
        }
    }
    // parent match
    for (int i = 1; i < sp->ninodes; i++) {
        if (i != 1 && dip[i].type == T_DIR) {
            for (int j = 0; j < NDIRECT; j++) {
                if (dip[i].addrs[j] == 0) continue;
                struct xv6_d *d = (struct xv6_d *)(img + dip[i].addrs[j] * BSIZE);
                for (int k = 0; k < BSIZE / sizeof(struct xv6_d); k++) {
                    if (!strcmp(d[k].name, "..")) {
                        struct dinode par = dip[d[k].inum];
                        int cnt = 0;
                        for (int n = 0; n < NDIRECT; n++) {
                            if (par.addrs[n] == 0) continue;
                            struct xv6_d *d2 = (struct xv6_d *)(img + par.addrs[n] * BSIZE);
                            for (int m = 0; m < BSIZE / sizeof(struct xv6_d); m++) {
                                if (d2[m].inum == 1) cnt = 1;
                            }
                        }
                        if (cnt == 0) {
                            err(ERR_PAR_MISMATCH);
                        }
                    }
                }
            }
        }
    }
}

int main(int argc, char *argv[]) {
    int fd;
    if (argc == 2) {
        fd = open(argv[1], O_RDONLY);
    } else {
        err(ERR_USAGE);
    }
    if (fd < 0) {
        err(ERR_IMG_FOUND);
    }

    struct stat buff;
    fstat(fd, &buff);
    img = mmap(NULL, buff.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (*((int *)img) == -1) {
        err(ERR_MMAP);
    }

    sp = (struct superblock *)(img + BSIZE);
    if (sp->size <= (sp->nblocks + sp->ninodes / 8 + 3)) {
        err(ERR_SP_BLK);
    }
    bitmap = (void *)(img + 3 * BSIZE + (sp->ninodes / IPB) * BSIZE);
    s_blk = bitmap + (((sp->nblocks) / (BSIZE * 8)));
    if ((sp->nblocks) % (BSIZE * 8) > 0) {
        s_blk = s_blk + BSIZE;
    }
    s_idx = (s_blk - img) / BSIZE;
    blocks = (int *)calloc(sp->size, sizeof(int));
    inodes = (int *)calloc(sp->ninodes, sizeof(int));
    refs = (int *)calloc(sp->ninodes, sizeof(int));
    dip = (struct dinode *)(img + 2 * BSIZE);
    s_bp = (char *)(img + 3 * BSIZE + ((sp->ninodes / IPB) * BSIZE));
    filesys_check();
    filesys_extra_check();
    free(blocks);
    free(inodes);
    free(refs);
    return 0;
}
