/*-
 * Copyright (c) 1998 Matthew Dillon.  All Rights Reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * BLIST.C -	Bitmap allocator/deallocator, using a radix tree with hinting
 *
 *	This module implements a general bitmap allocator/deallocator.  The
 *	allocator eats around 2 bits per 'block'.  The module does not 
 *	try to interpret the meaning of a 'block' other than to return 
 *	SWAPBLK_NONE on an allocation failure.
 *
 *	A radix tree is used to maintain the bitmap.  Two radix constants are
 *	involved:  One for the bitmaps contained in the leaf nodes (typically
 *	32), and one for the meta nodes (typically 16).  Both meta and leaf
 *	nodes have a hint field.  This field gives us a hint as to the largest
 *	free contiguous range of blocks under the node.  It may contain a
 *	value that is too high, but will never contain a value that is too 
 *	low.  When the radix tree is searched, allocation failures in subtrees
 *	update the hint. 
 *
 *	The radix tree also implements two collapsed states for meta nodes:
 *	the ALL-ALLOCATED state and the ALL-FREE state.  If a meta node is
 *	in either of these two states, all information contained underneath
 *	the node is considered stale.  These states are used to optimize
 *	allocation and freeing operations.
 *
 * 	The hinting greatly increases code efficiency for allocations while
 *	the general radix structure optimizes both allocations and frees.  The
 *	radix tree should be able to operate well no matter how much 
 *	fragmentation there is and no matter how large a bitmap is used.
 *
 *	The blist code wires all necessary memory at creation time.  Neither
 *	allocations nor frees require interaction with the memory subsystem.
 *	The non-blocking features of the blist code are used in the swap code
 *	(vm/swap_pager.c).
 *
 *	LAYOUT: The radix tree is laid out recursively using a
 *	linear array.  Each meta node is immediately followed (laid out
 *	sequentially in memory) by BLIST_META_RADIX lower level nodes.  This
 *	is a recursive structure but one that can be easily scanned through
 *	a very simple 'skip' calculation.  In order to support large radixes, 
 *	portions of the tree may reside outside our memory allocation.  We 
 *	handle this with an early-termination optimization (when bighint is 
 *	set to -1) on the scan.  The memory allocation is only large enough 
 *	to cover the number of blocks requested at creation time even if it
 *	must be encompassed in larger root-node radix.
 *
 *	NOTE: the allocator cannot currently allocate more than 
 *	BLIST_BMAP_RADIX blocks per call.  It will panic with 'allocation too 
 *	large' if you try.  This is an area that could use improvement.  The 
 *	radix is large enough that this restriction does not effect the swap 
 *	system, though.  Currently only the allocation code is effected by
 *	this algorithmic unfeature.  The freeing code can handle arbitrary
 *	ranges.
 *
 *	This code can be compiled stand-alone for debugging.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: head/sys/kern/subr_blist.c 298819 2016-04-29 22:15:33Z pfg $");

#ifdef _KERNEL

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/kernel.h>
#include <sys/blist.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/mutex.h> 

#else

#ifndef BLIST_NO_DEBUG
#define BLIST_DEBUG
#endif

#define SWAPBLK_NONE ((daddr_t)-1)

#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#define malloc(a,b,c)	calloc(a, 1)
#define free(a,b)	free(a)

typedef unsigned int u_daddr_t;

#include <sys/blist.h>

void panic(const char *ctl, ...);

#endif

/*
 * static support functions
 */

static daddr_t blst_leaf_alloc(blmeta_t *scan, daddr_t blk, int count);
static daddr_t blst_meta_alloc(blmeta_t *scan, daddr_t blk, 
				daddr_t count, daddr_t radix, int skip);
static void blst_leaf_free(blmeta_t *scan, daddr_t relblk, int count);
static void blst_meta_free(blmeta_t *scan, daddr_t freeBlk, daddr_t count, 
					daddr_t radix, int skip, daddr_t blk);
static void blst_copy(blmeta_t *scan, daddr_t blk, daddr_t radix, 
				daddr_t skip, blist_t dest, daddr_t count);
static int blst_leaf_fill(blmeta_t *scan, daddr_t blk, int count);
static int blst_meta_fill(blmeta_t *scan, daddr_t allocBlk, daddr_t count,
				daddr_t radix, int skip, daddr_t blk);
static daddr_t	blst_radix_init(blmeta_t *scan, daddr_t radix, 
						int skip, daddr_t count);
#ifndef _KERNEL
static void	blst_radix_print(blmeta_t *scan, daddr_t blk, 
					daddr_t radix, int skip, int tab);
#endif

#ifdef _KERNEL
static MALLOC_DEFINE(M_SWAP, "SWAP", "Swap space");
#endif

/*
 * blist_create() - create a blist capable of handling up to the specified
 *		    number of blocks
 *
 *	blocks - must be greater than 0
 * 	flags  - malloc flags
 *
 *	The smallest blist consists of a single leaf node capable of 
 *	managing BLIST_BMAP_RADIX blocks.
 */

blist_t 
blist_create(daddr_t blocks, int flags)
{
	blist_t bl;
	int radix;
	int skip = 0;

	/*
	 * Calculate radix and skip field used for scanning.
	 */
	radix = BLIST_BMAP_RADIX;

	while (radix < blocks) {
		radix *= BLIST_META_RADIX;
		skip = (skip + 1) * BLIST_META_RADIX;
	}

	bl = malloc(sizeof(struct blist), M_SWAP, flags | M_ZERO);

	bl->bl_blocks = blocks;
	bl->bl_radix = radix;
	bl->bl_skip = skip;
	bl->bl_rootblks = 1 +
	    blst_radix_init(NULL, bl->bl_radix, bl->bl_skip, blocks);
	bl->bl_root = malloc(sizeof(blmeta_t) * bl->bl_rootblks, M_SWAP, flags);

#if defined(BLIST_DEBUG)
	printf(
		"BLIST representing %lld blocks (%lld MB of swap)"
		", requiring %lldK of ram\n",
		(long long)bl->bl_blocks,
		(long long)bl->bl_blocks * 4 / 1024,
		(long long)(bl->bl_rootblks * sizeof(blmeta_t) + 1023) / 1024
	);
	printf("BLIST raw radix tree contains %lld records\n",
	    (long long)bl->bl_rootblks);
#endif
	blst_radix_init(bl->bl_root, bl->bl_radix, bl->bl_skip, blocks);

	return(bl);
}

void 
blist_destroy(blist_t bl)
{
	free(bl->bl_root, M_SWAP);
	free(bl, M_SWAP);
}

/*
 * blist_alloc() - reserve space in the block bitmap.  Return the base
 *		     of a contiguous region or SWAPBLK_NONE if space could
 *		     not be allocated.
 */

daddr_t 
blist_alloc(blist_t bl, daddr_t count)
{
	daddr_t blk = SWAPBLK_NONE;

	if (bl) {
		if (bl->bl_radix == BLIST_BMAP_RADIX)
			blk = blst_leaf_alloc(bl->bl_root, 0, count);
		else
			blk = blst_meta_alloc(bl->bl_root, 0, count, bl->bl_radix, bl->bl_skip);
		if (blk != SWAPBLK_NONE)
			bl->bl_free -= count;
	}
	return(blk);
}

/*
 * blist_free() -	free up space in the block bitmap.  Return the base
 *		     	of a contiguous region.  Panic if an inconsistancy is
 *			found.
 */

void 
blist_free(blist_t bl, daddr_t blkno, daddr_t count)
{
	if (bl) {
		if (bl->bl_radix == BLIST_BMAP_RADIX)
			blst_leaf_free(bl->bl_root, blkno, count);
		else
			blst_meta_free(bl->bl_root, blkno, count, bl->bl_radix, bl->bl_skip, 0);
		bl->bl_free += count;
	}
}

/*
 * blist_fill() -	mark a region in the block bitmap as off-limits
 *			to the allocator (i.e. allocate it), ignoring any
 *			existing allocations.  Return the number of blocks
 *			actually filled that were free before the call.
 */

int
blist_fill(blist_t bl, daddr_t blkno, daddr_t count)
{
	int filled;

	if (bl) {
		if (bl->bl_radix == BLIST_BMAP_RADIX)
			filled = blst_leaf_fill(bl->bl_root, blkno, count);
		else
			filled = blst_meta_fill(bl->bl_root, blkno, count,
			    bl->bl_radix, bl->bl_skip, 0);
		bl->bl_free -= filled;
		return filled;
	} else
		return 0;
}

/*
 * blist_resize() -	resize an existing radix tree to handle the
 *			specified number of blocks.  This will reallocate
 *			the tree and transfer the previous bitmap to the new
 *			one.  When extending the tree you can specify whether
 *			the new blocks are to left allocated or freed.
 */

void
blist_resize(blist_t *pbl, daddr_t count, int freenew, int flags)
{
    blist_t newbl = blist_create(count, flags);
    blist_t save = *pbl;

    *pbl = newbl;
    if (count > save->bl_blocks)
	    count = save->bl_blocks;
    blst_copy(save->bl_root, 0, save->bl_radix, save->bl_skip, newbl, count);

    /*
     * If resizing upwards, should we free the new space or not?
     */
    if (freenew && count < newbl->bl_blocks) {
	    blist_free(newbl, count, newbl->bl_blocks - count);
    }
    blist_destroy(save);
}

#ifdef BLIST_DEBUG

/*
 * blist_print()    - dump radix tree
 */

void
blist_print(blist_t bl)
{
	printf("BLIST {\n");
	blst_radix_print(bl->bl_root, 0, bl->bl_radix, bl->bl_skip, 4);
	printf("}\n");
}

#endif

/************************************************************************
 *			  ALLOCATION SUPPORT FUNCTIONS			*
 ************************************************************************
 *
 *	These support functions do all the actual work.  They may seem 
 *	rather longish, but that's because I've commented them up.  The
 *	actual code is straight forward.
 *
 */

/*
 * blist_leaf_alloc() -	allocate at a leaf in the radix tree (a bitmap).
 *
 *	This is the core of the allocator and is optimized for the 1 block
 *	and the BLIST_BMAP_RADIX block allocation cases.  Other cases are
 *	somewhat slower.  The 1 block allocation case is log2 and extremely
 *	quick.
 */

static daddr_t
blst_leaf_alloc(
	blmeta_t *scan,
	daddr_t blk,
	int count
) {
	u_daddr_t orig = scan->u.bmu_bitmap;

	if (orig == 0) {
		/*
		 * Optimize bitmap all-allocated case.  Also, count = 1
		 * case assumes at least 1 bit is free in the bitmap, so
		 * we have to take care of this case here.
		 */
		scan->bm_bighint = 0;
		return(SWAPBLK_NONE);
	}
	if (count == 1) {
		/*
		 * Optimized code to allocate one bit out of the bitmap
		 */
		u_daddr_t mask;
		int j = BLIST_BMAP_RADIX/2;
		int r = 0;

		mask = (u_daddr_t)-1 >> (BLIST_BMAP_RADIX/2);

		while (j) {
			if ((orig & mask) == 0) {
			    r += j;
			    orig >>= j;
			}
			j >>= 1;
			mask >>= j;
		}
		scan->u.bmu_bitmap &= ~(1 << r);
		return(blk + r);
	}
	if (count <= BLIST_BMAP_RADIX) {
		/*
		 * non-optimized code to allocate N bits out of the bitmap.
		 * The more bits, the faster the code runs.  It will run
		 * the slowest allocating 2 bits, but since there aren't any
		 * memory ops in the core loop (or shouldn't be, anyway),
		 * you probably won't notice the difference.
		 */
		int j;
		int n = BLIST_BMAP_RADIX - count;
		u_daddr_t mask;

		mask = (u_daddr_t)-1 >> n;

		for (j = 0; j <= n; ++j) {
			if ((orig & mask) == mask) {
				scan->u.bmu_bitmap &= ~mask;
				return(blk + j);
			}
			mask = (mask << 1);
		}
	}
	/*
	 * We couldn't allocate count in this subtree, update bighint.
	 */
	scan->bm_bighint = count - 1;
	return(SWAPBLK_NONE);
}

/*
 * blist_meta_alloc() -	allocate at a meta in the radix tree.
 *
 *	Attempt to allocate at a meta node.  If we can't, we update
 *	bighint and return a failure.  Updating bighint optimize future
 *	calls that hit this node.  We have to check for our collapse cases
 *	and we have a few optimizations strewn in as well.
 */

static daddr_t
blst_meta_alloc(
	blmeta_t *scan, 
	daddr_t blk,
	daddr_t count,
	daddr_t radix, 
	int skip
) {
	int i;
	int next_skip = ((u_int)skip / BLIST_META_RADIX);

	if (scan->u.bmu_avail == 0)  {
		/*
		 * ALL-ALLOCATED special case
		 */
		scan->bm_bighint = count;
		return(SWAPBLK_NONE);
	}

	if (scan->u.bmu_avail == radix) {
		radix /= BLIST_META_RADIX;

		/*
		 * ALL-FREE special case, initialize uninitialize
		 * sublevel.
		 */
		for (i = 1; i <= skip; i += next_skip) {
			if (scan[i].bm_bighint == (daddr_t)-1)
				break;
			if (next_skip == 1) {
				scan[i].u.bmu_bitmap = (u_daddr_t)-1;
				scan[i].bm_bighint = BLIST_BMAP_RADIX;
			} else {
				scan[i].bm_bighint = radix;
				scan[i].u.bmu_avail = radix;
			}
		}
	} else {
		radix /= BLIST_META_RADIX;
	}

	for (i = 1; i <= skip; i += next_skip) {
		if (count <= scan[i].bm_bighint) {
			/*
			 * count fits in object
			 */
			daddr_t r;
			if (next_skip == 1) {
				r = blst_leaf_alloc(&scan[i], blk, count);
			} else {
				r = blst_meta_alloc(&scan[i], blk, count, radix, next_skip - 1);
			}
			if (r != SWAPBLK_NONE) {
				scan->u.bmu_avail -= count;
				if (scan->bm_bighint > scan->u.bmu_avail)
					scan->bm_bighint = scan->u.bmu_avail;
				return(r);
			}
		} else if (scan[i].bm_bighint == (daddr_t)-1) {
			/*
			 * Terminator
			 */
			break;
		} else if (count > radix) {
			/*
			 * count does not fit in object even if it were
			 * complete free.
			 */
			panic("blist_meta_alloc: allocation too large");
		}
		blk += radix;
	}

	/*
	 * We couldn't allocate count in this subtree, update bighint.
	 */
	if (scan->bm_bighint >= count)
		scan->bm_bighint = count - 1;
	return(SWAPBLK_NONE);
}

/*
 * BLST_LEAF_FREE() -	free allocated block from leaf bitmap
 *
 */

static void
blst_leaf_free(
	blmeta_t *scan,
	daddr_t blk,
	int count
) {
	/*
	 * free some data in this bitmap
	 *
	 * e.g.
	 *	0000111111111110000
	 *          \_________/\__/
	 *		v        n
	 */
	int n = blk & (BLIST_BMAP_RADIX - 1);
	u_daddr_t mask;

	mask = ((u_daddr_t)-1 << n) &
	    ((u_daddr_t)-1 >> (BLIST_BMAP_RADIX - count - n));

	if (scan->u.bmu_bitmap & mask)
		panic("blst_radix_free: freeing free block");
	scan->u.bmu_bitmap |= mask;

	/*
	 * We could probably do a better job here.  We are required to make
	 * bighint at least as large as the biggest contiguous block of 
	 * data.  If we just shoehorn it, a little extra overhead will
	 * be incured on the next allocation (but only that one typically).
	 */
	scan->bm_bighint = BLIST_BMAP_RADIX;
}

/*
 * BLST_META_FREE() - free allocated blocks from radix tree meta info
 *
 *	This support routine frees a range of blocks from the bitmap.
 *	The range must be entirely enclosed by this radix node.  If a
 *	meta node, we break the range down recursively to free blocks
 *	in subnodes (which means that this code can free an arbitrary
 *	range whereas the allocation code cannot allocate an arbitrary
 *	range).
 */

static void 
blst_meta_free(
	blmeta_t *scan, 
	daddr_t freeBlk,
	daddr_t count,
	daddr_t radix, 
	int skip,
	daddr_t blk
) {
	int i;
	int next_skip = ((u_int)skip / BLIST_META_RADIX);

#if 0
	printf("free (%llx,%lld) FROM (%llx,%lld)\n",
	    (long long)freeBlk, (long long)count,
	    (long long)blk, (long long)radix
	);
#endif

	if (scan->u.bmu_avail == 0) {
		/*
		 * ALL-ALLOCATED special case, with possible
		 * shortcut to ALL-FREE special case.
		 */
		scan->u.bmu_avail = count;
		scan->bm_bighint = count;

		if (count != radix)  {
			for (i = 1; i <= skip; i += next_skip) {
				if (scan[i].bm_bighint == (daddr_t)-1)
					break;
				scan[i].bm_bighint = 0;
				if (next_skip == 1) {
					scan[i].u.bmu_bitmap = 0;
				} else {
					scan[i].u.bmu_avail = 0;
				}
			}
			/* fall through */
		}
	} else {
		scan->u.bmu_avail += count;
		/* scan->bm_bighint = radix; */
	}

	/*
	 * ALL-FREE special case.
	 */

	if (scan->u.bmu_avail == radix)
		return;
	if (scan->u.bmu_avail > radix)
		panic("blst_meta_free: freeing already free blocks (%lld) %lld/%lld",
		    (long long)count, (long long)scan->u.bmu_avail,
		    (long long)radix);

	/*
	 * Break the free down into its components
	 */

	radix /= BLIST_META_RADIX;

	i = (freeBlk - blk) / radix;
	blk += i * radix;
	i = i * next_skip + 1;

	while (i <= skip && blk < freeBlk + count) {
		daddr_t v;

		v = blk + radix - freeBlk;
		if (v > count)
			v = count;

		if (scan->bm_bighint == (daddr_t)-1)
			panic("blst_meta_free: freeing unexpected range");

		if (next_skip == 1) {
			blst_leaf_free(&scan[i], freeBlk, v);
		} else {
			blst_meta_free(&scan[i], freeBlk, v, radix, next_skip - 1, blk);
		}
		if (scan->bm_bighint < scan[i].bm_bighint)
		    scan->bm_bighint = scan[i].bm_bighint;
		count -= v;
		freeBlk += v;
		blk += radix;
		i += next_skip;
	}
}

/*
 * BLIST_RADIX_COPY() - copy one radix tree to another
 *
 *	Locates free space in the source tree and frees it in the destination
 *	tree.  The space may not already be free in the destination.
 */

static void blst_copy(
	blmeta_t *scan, 
	daddr_t blk,
	daddr_t radix, 
	daddr_t skip, 
	blist_t dest,
	daddr_t count
) {
	int next_skip;
	int i;

	/*
	 * Leaf node
	 */

	if (radix == BLIST_BMAP_RADIX) {
		u_daddr_t v = scan->u.bmu_bitmap;

		if (v == (u_daddr_t)-1) {
			blist_free(dest, blk, count);
		} else if (v != 0) {
			int i;

			for (i = 0; i < BLIST_BMAP_RADIX && i < count; ++i) {
				if (v & (1 << i))
					blist_free(dest, blk + i, 1);
			}
		}
		return;
	}

	/*
	 * Meta node
	 */

	if (scan->u.bmu_avail == 0) {
		/*
		 * Source all allocated, leave dest allocated
		 */
		return;
	} 
	if (scan->u.bmu_avail == radix) {
		/*
		 * Source all free, free entire dest
		 */
		if (count < radix)
			blist_free(dest, blk, count);
		else
			blist_free(dest, blk, radix);
		return;
	}


	radix /= BLIST_META_RADIX;
	next_skip = ((u_int)skip / BLIST_META_RADIX);

	for (i = 1; count && i <= skip; i += next_skip) {
		if (scan[i].bm_bighint == (daddr_t)-1)
			break;

		if (count >= radix) {
			blst_copy(
			    &scan[i],
			    blk,
			    radix,
			    next_skip - 1,
			    dest,
			    radix
			);
			count -= radix;
		} else {
			if (count) {
				blst_copy(
				    &scan[i],
				    blk,
				    radix,
				    next_skip - 1,
				    dest,
				    count
				);
			}
			count = 0;
		}
		blk += radix;
	}
}

/*
 * BLST_LEAF_FILL() -	allocate specific blocks in leaf bitmap
 *
 *	This routine allocates all blocks in the specified range
 *	regardless of any existing allocations in that range.  Returns
 *	the number of blocks allocated by the call.
 */

static int
blst_leaf_fill(blmeta_t *scan, daddr_t blk, int count)
{
	int n = blk & (BLIST_BMAP_RADIX - 1);
	int nblks;
	u_daddr_t mask, bitmap;

	mask = ((u_daddr_t)-1 << n) &
	    ((u_daddr_t)-1 >> (BLIST_BMAP_RADIX - count - n));

	/* Count the number of blocks we're about to allocate */
	bitmap = scan->u.bmu_bitmap & mask;
	for (nblks = 0; bitmap != 0; nblks++)
		bitmap &= bitmap - 1;

	scan->u.bmu_bitmap &= ~mask;
	return nblks;
}

/*
 * BLIST_META_FILL() -	allocate specific blocks at a meta node
 *
 *	This routine allocates the specified range of blocks,
 *	regardless of any existing allocations in the range.  The
 *	range must be within the extent of this node.  Returns the
 *	number of blocks allocated by the call.
 */
static int
blst_meta_fill(
	blmeta_t *scan,
	daddr_t allocBlk,
	daddr_t count,
	daddr_t radix, 
	int skip,
	daddr_t blk
) {
	int i;
	int next_skip = ((u_int)skip / BLIST_META_RADIX);
	int nblks = 0;

	if (count == radix || scan->u.bmu_avail == 0)  {
		/*
		 * ALL-ALLOCATED special case
		 */
		nblks = scan->u.bmu_avail;
		scan->u.bmu_avail = 0;
		scan->bm_bighint = count;
		return nblks;
	}

	if (scan->u.bmu_avail == radix) {
		radix /= BLIST_META_RADIX;

		/*
		 * ALL-FREE special case, initialize sublevel
		 */
		for (i = 1; i <= skip; i += next_skip) {
			if (scan[i].bm_bighint == (daddr_t)-1)
				break;
			if (next_skip == 1) {
				scan[i].u.bmu_bitmap = (u_daddr_t)-1;
				scan[i].bm_bighint = BLIST_BMAP_RADIX;
			} else {
				scan[i].bm_bighint = radix;
				scan[i].u.bmu_avail = radix;
			}
		}
	} else {
		radix /= BLIST_META_RADIX;
	}

	if (count > radix)
		panic("blist_meta_fill: allocation too large");

	i = (allocBlk - blk) / radix;
	blk += i * radix;
	i = i * next_skip + 1;

	while (i <= skip && blk < allocBlk + count) {
		daddr_t v;

		v = blk + radix - allocBlk;
		if (v > count)
			v = count;

		if (scan->bm_bighint == (daddr_t)-1)
			panic("blst_meta_fill: filling unexpected range");

		if (next_skip == 1) {
			nblks += blst_leaf_fill(&scan[i], allocBlk, v);
		} else {
			nblks += blst_meta_fill(&scan[i], allocBlk, v,
			    radix, next_skip - 1, blk);
		}
		count -= v;
		allocBlk += v;
		blk += radix;
		i += next_skip;
	}
	scan->u.bmu_avail -= nblks;
	return nblks;
}

/*
 * BLST_RADIX_INIT() - initialize radix tree
 *
 *	Initialize our meta structures and bitmaps and calculate the exact
 *	amount of space required to manage 'count' blocks - this space may
 *	be considerably less than the calculated radix due to the large
 *	RADIX values we use.
 */

static daddr_t	
blst_radix_init(blmeta_t *scan, daddr_t radix, int skip, daddr_t count)
{
	int i;
	int next_skip;
	daddr_t memindex = 0;

	/*
	 * Leaf node
	 */

	if (radix == BLIST_BMAP_RADIX) {
		if (scan) {
			scan->bm_bighint = 0;
			scan->u.bmu_bitmap = 0;
		}
		return(memindex);
	}

	/*
	 * Meta node.  If allocating the entire object we can special
	 * case it.  However, we need to figure out how much memory
	 * is required to manage 'count' blocks, so we continue on anyway.
	 */

	if (scan) {
		scan->bm_bighint = 0;
		scan->u.bmu_avail = 0;
	}

	radix /= BLIST_META_RADIX;
	next_skip = ((u_int)skip / BLIST_META_RADIX);

	for (i = 1; i <= skip; i += next_skip) {
		if (count >= radix) {
			/*
			 * Allocate the entire object
			 */
			memindex = i + blst_radix_init(
			    ((scan) ? &scan[i] : NULL),
			    radix,
			    next_skip - 1,
			    radix
			);
			count -= radix;
		} else if (count > 0) {
			/*
			 * Allocate a partial object
			 */
			memindex = i + blst_radix_init(
			    ((scan) ? &scan[i] : NULL),
			    radix,
			    next_skip - 1,
			    count
			);
			count = 0;
		} else {
			/*
			 * Add terminator and break out
			 */
			if (scan)
				scan[i].bm_bighint = (daddr_t)-1;
			break;
		}
	}
	if (memindex < i)
		memindex = i;
	return(memindex);
}

#ifdef BLIST_DEBUG

static void	
blst_radix_print(blmeta_t *scan, daddr_t blk, daddr_t radix, int skip, int tab)
{
	int i;
	int next_skip;
	int lastState = 0;

	if (radix == BLIST_BMAP_RADIX) {
		printf(
		    "%*.*s(%08llx,%lld): bitmap %08llx big=%lld\n", 
		    tab, tab, "",
		    (long long)blk, (long long)radix,
		    (long long)scan->u.bmu_bitmap,
		    (long long)scan->bm_bighint
		);
		return;
	}

	if (scan->u.bmu_avail == 0) {
		printf(
		    "%*.*s(%08llx,%lld) ALL ALLOCATED\n",
		    tab, tab, "",
		    (long long)blk,
		    (long long)radix
		);
		return;
	}
	if (scan->u.bmu_avail == radix) {
		printf(
		    "%*.*s(%08llx,%lld) ALL FREE\n",
		    tab, tab, "",
		    (long long)blk,
		    (long long)radix
		);
		return;
	}

	printf(
	    "%*.*s(%08llx,%lld): subtree (%lld/%lld) big=%lld {\n",
	    tab, tab, "",
	    (long long)blk, (long long)radix,
	    (long long)scan->u.bmu_avail,
	    (long long)radix,
	    (long long)scan->bm_bighint
	);

	radix /= BLIST_META_RADIX;
	next_skip = ((u_int)skip / BLIST_META_RADIX);
	tab += 4;

	for (i = 1; i <= skip; i += next_skip) {
		if (scan[i].bm_bighint == (daddr_t)-1) {
			printf(
			    "%*.*s(%08llx,%lld): Terminator\n",
			    tab, tab, "",
			    (long long)blk, (long long)radix
			);
			lastState = 0;
			break;
		}
		blst_radix_print(
		    &scan[i],
		    blk,
		    radix,
		    next_skip - 1,
		    tab
		);
		blk += radix;
	}
	tab -= 4;

	printf(
	    "%*.*s}\n",
	    tab, tab, ""
	);
}

#endif

#ifdef BLIST_DEBUG

int
main(int ac, char **av)
{
	int size = 1024;
	int i;
	blist_t bl;

	for (i = 1; i < ac; ++i) {
		const char *ptr = av[i];
		if (*ptr != '-') {
			size = strtol(ptr, NULL, 0);
			continue;
		}
		ptr += 2;
		fprintf(stderr, "Bad option: %s\n", ptr - 2);
		exit(1);
	}
	bl = blist_create(size, M_WAITOK);
	blist_free(bl, 0, size);

	for (;;) {
		char buf[1024];
		daddr_t da = 0;
		daddr_t count = 0;


		printf("%lld/%lld/%lld> ", (long long)bl->bl_free,
		    (long long)size, (long long)bl->bl_radix);
		fflush(stdout);
		if (fgets(buf, sizeof(buf), stdin) == NULL)
			break;
		switch(buf[0]) {
		case 'r':
			if (sscanf(buf + 1, "%lld", &count) == 1) {
				blist_resize(&bl, count, 1);
			} else {
				printf("?\n");
			}
		case 'p':
			blist_print(bl);
			break;
		case 'a':
			if (sscanf(buf + 1, "%lld", &count) == 1) {
				daddr_t blk = blist_alloc(bl, count);
				printf("    R=%08llx\n", (long long)blk);
			} else {
				printf("?\n");
			}
			break;
		case 'f':
			if (sscanf(buf + 1, "%llx %lld",
			    (long long *)&da, (long long *)&count) == 2) {
				blist_free(bl, da, count);
			} else {
				printf("?\n");
			}
			break;
		case 'l':
			if (sscanf(buf + 1, "%llx %lld",
			    (long long *)&da, (long long *)&count) == 2) {
				printf("    n=%d\n",
				    blist_fill(bl, da, count));
			} else {
				printf("?\n");
			}
			break;
		case '?':
		case 'h':
			puts(
			    "p          -print\n"
			    "a %d       -allocate\n"
			    "f %x %d    -free\n"
			    "l %x %d    -fill\n"
			    "r %d       -resize\n"
			    "h/?        -help"
			);
			break;
		default:
			printf("?\n");
			break;
		}
	}
	return(0);
}

void
panic(const char *ctl, ...)
{
	va_list va;

	va_start(va, ctl);
	vfprintf(stderr, ctl, va);
	fprintf(stderr, "\n");
	va_end(va);
	exit(1);
}

#endif

