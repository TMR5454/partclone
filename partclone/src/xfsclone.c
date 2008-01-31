/**
 * xfsclone.c - part of Partclone project
 *
 * Copyright (c) 2007~ Thomas Tsai <thomas at nchc org tw>
 *
 * read xfs super block and bitmap
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdint.h>
#include <malloc.h>
#include <stdarg.h>
#include <getopt.h>
#include <xfs/libxfs.h>
#include "partclone.h"
#include "xfsclone.h"

char *EXECNAME = "clone.xfs";

libxfs_init_t   x;
xfs_mount_t     xmount;
xfs_mount_t     *mp;

static void addToHist(int dwAgNo, int dwAgBlockNo, int qwLen, char* bitmap)
{
    int bit;
    int qwBase, i;
    int debug = 2;

    qwBase = (dwAgNo * mp->m_sb.sb_agblocks) + dwAgBlockNo;
    log_mesg(2, 0, 0, debug, "addTohits:%i,\t%i,\t%i,\t%i\n",dwAgNo, dwAgBlockNo, qwLen, qwBase);
    for (i = 0; i < qwLen; i++)
    {
	bit = qwBase + i -1;
	bitmap[bit] = 0;
	log_mesg(3, 0, 0, debug, "add bit%i\n",bit);
    }

}

static void scanfunc_bno(xfs_btree_sblock_t* ablock,  int level, xfs_agf_t* agf, char* bitmap) 
{
    xfs_alloc_block_t	*block = (xfs_alloc_block_t *)ablock;
    int			i;
    xfs_alloc_ptr_t 	*pp;
    xfs_alloc_rec_t	*rp;
    int			debug = 2;

    if (level == 0) 
    {
	rp = XFS_BTREE_REC_ADDR(mp->m_sb.sb_blocksize, xfs_alloc, block, 1, mp->m_alloc_mxr[0]);
	for (i = 0; i < INT_GET(block->bb_numrecs, ARCH_CONVERT); i++){
	    log_mesg(2, 0, 0, debug, "scan:%i,\t%i,\t%i\n", (int)INT_GET(agf->agf_seqno, ARCH_CONVERT), (int)INT_GET(rp[i].ar_startblock, ARCH_CONVERT), (int)INT_GET(rp[i].ar_blockcount, ARCH_CONVERT));
	    addToHist((int)INT_GET(agf->agf_seqno, ARCH_CONVERT), (int)INT_GET(rp[i].ar_startblock, ARCH_CONVERT), (int)INT_GET(rp[i].ar_blockcount, ARCH_CONVERT), bitmap);
	}
	return;
    }
    pp = XFS_BTREE_PTR_ADDR(mp->m_sb.sb_blocksize, xfs_alloc, block, 1, mp->m_alloc_mxr[1]);
    for (i = 0; i < INT_GET(block->bb_numrecs, ARCH_CONVERT); i++) 
    {
	scan_sbtree(agf, INT_GET(pp[i], ARCH_CONVERT), level, bitmap);
    }
}

static void scan_sbtree(xfs_agf_t* agf, xfs_agblock_t root, int nlevels, char* bitmap)
{
    xfs_agnumber_t      seqno = INT_GET(agf->agf_seqno, ARCH_CONVERT);
    void                *btree_bufp = NULL;
    xfs_btree_sblock_t  *data;
    int                 c, i;
    int			debug = 2;

    //read_bbs
    c = BBTOB(1 << mp->m_blkbb_log);
    btree_bufp = valloc(c);
    lseek64(x.dfd, XFS_AGB_TO_DADDR(mp, seqno, root) << BBSHIFT, SEEK_SET);
	
    i = (int)read(x.dfd, btree_bufp, c);
    if(i < 0 || i < c)
	log_mesg(0, 1, 1, debug, "read bbs err\n");
    //end read_bbs
    data = (xfs_btree_sblock_t *)btree_bufp;
    //set_sur end
 
    scanfunc_bno(data, nlevels - 1, agf, bitmap);
    free(btree_bufp);
}

static void fs_open(char* device)
{

    xfs_sb_t        *sbp;
    xfs_agnumber_t  agno = 0;
    void            *bufp = NULL;
    int             c, i;
    int		    debug = 2;

    x.dname = device;
    if (!libxfs_init(&x)) 
    {
	log_mesg(0, 1, 1, debug, "SUPER: libxfs_init error\n");
    }

    c = BBTOB(1);
    bufp = valloc(c);
    if (lseek64(x.dfd, XFS_SB_DADDR << BBSHIFT, SEEK_SET) < 0) 
    {
	log_mesg(0, 1, 1, debug, "seek error\n");
    }

    i = (int)read(x.dfd, bufp, c);
    if(i < 0 || i < c)
    {
	log_mesg(0, 1, 1, debug, "read bbs err\n");
    }
    libxfs_xlate_sb(bufp, &xmount.m_sb, 1, XFS_SB_ALL_BITS);
    free(bufp);
    sbp = &xmount.m_sb;
    if (sbp->sb_magicnum != XFS_SB_MAGIC) 
    {
	log_mesg(0, 1, 1, debug, "magic(0x%08x) error\n", sbp->sb_magicnum);
    }
    
    if (!XFS_SB_GOOD_VERSION(sbp)) 
    {
	log_mesg(0, 1, 1, debug, "bad sb version\n");
    }

    mp = libxfs_mount(&xmount, sbp, x.ddev, x.logdev, x.rtdev, LIBXFS_MOUNT_ROOTINOS);
    if(!mp)
    {
	mp = libxfs_mount(&xmount, sbp, x.ddev, x.logdev, x.rtdev, LIBXFS_MOUNT_DEBUGGER);
	if (!mp) 
	{
	    log_mesg(0, 1, 1, debug, "device %s unusable (not an XFS filesystem?)\n", x.dname);
	}
    } else {
	log_mesg(0, 0, 0, debug, "device %s usable (XFS filesystem)\n", x.dname);
    }

}

static void fs_close()
{
    libxfs_device_close(x.ddev);
}

extern void initial_image_hdr(char* device, image_head* image_hdr)
{
    fs_open(device);
    memcpy(image_hdr->magic, IMAGE_MAGIC, IMAGE_MAGIC_SIZE);
    memcpy(image_hdr->fs, xfs_MAGIC, FS_MAGIC_SIZE);
    image_hdr->block_size = (int)mp->m_sb.sb_blocksize;
    image_hdr->totalblock = (unsigned long long)mp->m_sb.sb_dblocks;
    image_hdr->usedblocks = (unsigned long long)(mp->m_sb.sb_dblocks - mp->m_sb.sb_fdblocks);
    image_hdr->device_size = (unsigned long long)(mp->m_sb.sb_dblocks * mp->m_sb.sb_blocksize);
    fs_close();

}

extern void readbitmap(char* device, image_head image_hdr, char* bitmap)
{
    void            *agf_bufp = NULL;
    void            *agfl_bufp = NULL;
    int             i, c, b, seek, bit;
    xfs_agf_t       *agf;
    xfs_agfl_t      *agfl;
    xfs_agnumber_t  seqno;
    xfs_agblock_t   bno;
    xfs_agnumber_t  agno = 0;
    int             bfree = 0,  bused = 0;
    int		    debug = 2;


    fs_open(device);
    // init bitmap
    if (agno == 0 && xmount.m_sb.sb_inprogress != 0) 
    {
	log_mesg(0, 1, 1, debug, "mkfs not completed successfully\n");
    }

    log_mesg(2, 0, 0, debug, "initial bitmap as used\n");
    for(bit = 0; bit < mp->m_sb.sb_dblocks; bit++)
    {
	bitmap[bit] = 1;
    }
    
    for (agno = 0; (int)agno < (int)mp->m_sb.sb_agcount; agno++)
    {
	b = 0;
	c = BBTOB(XFS_FSS_TO_BB(mp, 1));
	agf_bufp = valloc(c);
	seek = lseek64(x.dfd, XFS_AG_DADDR(mp, agno, XFS_AGF_DADDR(mp)) << BBSHIFT, SEEK_SET);
	i = (int)read(x.dfd, agf_bufp, c);
	agf = (xfs_agf_t *)agf_bufp;
	seqno = INT_GET(agf->agf_seqno, ARCH_CONVERT);
	if (INT_GET(agf->agf_flcount, ARCH_CONVERT) > 0)
	{
	    c = BBTOB(XFS_FSS_TO_BB(mp, 1));
	    agfl_bufp = valloc(c);
	    lseek64(x.dfd, XFS_AG_DADDR(mp, seqno, XFS_AGFL_DADDR(mp)) << BBSHIFT, SEEK_SET);
	    i = (int)read(x.dfd, agfl_bufp, c);
	    agfl = (xfs_agfl_t *)agfl_bufp;
	    b = INT_GET(agf->agf_flfirst, ARCH_CONVERT);
	    for (;;) 
	    {
		bno = INT_GET(agfl->agfl_bno[b], ARCH_CONVERT);
		addToHist((int)seqno, (int)bno, 1, bitmap);
		log_mesg(2, 0, 0, debug, "%i,\t%i,\t%i\n",(int)seqno, (int)bno, 1);

		if (b == (int)INT_GET(agf->agf_fllast, ARCH_CONVERT))
		{
		    break;
		}
		if (++b == (int)XFS_AGFL_SIZE(mp))
		{
		    b = 0;
		}
	    }
	    free(agfl_bufp);
	}
      scan_sbtree(agf, INT_GET(agf->agf_roots[XFS_BTNUM_BNO], ARCH_CONVERT), INT_GET(agf->agf_levels[XFS_BTNUM_BNO],ARCH_CONVERT), bitmap);
      free(agf_bufp);
    }

    for(bit = 0; bit < mp->m_sb.sb_dblocks; bit++)
    {
        if (bitmap[bit] == 1)
	{
	    bused++;
	    log_mesg(3, 0, 0, debug, "used b= %i\n", bit);
	} else {
	    bfree++;
	    log_mesg(3, 0, 0, debug, "free b= %i\n", bit);
	}
    }
    //log_mesg(0, 0, 0, debug, "used = %i, free = %i\n", bused, bfree);

    if(bfree != mp->m_sb.sb_fdblocks)
        log_mesg(0, 1, 1, debug, "bitmap free count err, free:%i\n", bfree);

    fs_close();
//	image_hdr->usedblocks = bused;

}
