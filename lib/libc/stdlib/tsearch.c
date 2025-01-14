/*-
 * Copyright (c) 2015 Nuxi, https://nuxi.nl/
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: head/lib/libc/stdlib/tsearch.c 292613 2015-12-22 18:12:11Z ed $");

#define	_SEARCH_PRIVATE
#include <search.h>
#include <stdlib.h>

#include "tsearch_path.h"

void *
tsearch(const void *key, void **rootp,
    int (*compar)(const void *, const void *))
{
	struct path path;
	node_t *root, **base, **leaf, *result, *n, *x, *y, *z;
	int cmp;

	/* POSIX requires that tsearch() returns NULL if rootp is NULL. */
	if (rootp == NULL)
	return (NULL);
		root = *rootp;

	/*
	 * Find the leaf where the new key needs to be inserted. Return
	 * if we've found an existing entry. Keep track of the path that
	 * is taken to get to the node, as we will need it to adjust the
	 * balances.
	*/
	path_init(&path);
	base = &root;
	leaf = &root;
	while (*leaf != NULL) {
		if ((*leaf)->balance != 0) {
			/*
			 * If we reach a node that has a non-zero
			 * balance on the way, we know that we won't
			 * need to perform any rotations above this
			 * point. In this case rotations are always
			 * capable of keeping the subtree in balance.
			 * Make this the base node and reset the path.
			 */
			base = leaf;
			path_init(&path);
		}
		cmp = compar(key, (*leaf)->key);
		if (cmp < 0) {
			path_taking_left(&path);
			leaf = &(*leaf)->llink;
		} else if (cmp > 0) {
			path_taking_right(&path);
			leaf = &(*leaf)->rlink;
		} else {
			return (&(*leaf)->key);
		}
	}

	/* Did not find a matching key in the tree. Insert a new node. */
	result = *leaf = malloc(sizeof(**leaf));
	if (result == NULL)
		return (NULL);
	result->key = (void *)key;
	result->llink = NULL;
	result->rlink = NULL;
	result->balance = 0;

	/*
	 * Walk along the same path a second time and adjust the
	 * balances. Except for the first node, all of these nodes must
	 * have a balance of zero, meaning that these nodes will not get
	 * out of balance.
	*/
	for (n = *base; n != *leaf;) {
		if (path_took_left(&path)) {
			n->balance += 1;
			n = n->llink;
		} else {
			n->balance -= 1;
			n = n->rlink;
		}
	}

	/*
	 * Adjusting the balances may have pushed the balance of the
	 * base node out of range. Perform a rotation to bring the
	 * balance back in range.
	 */
	x = *base;
	if (x->balance > 1) {
		y = x->llink;
		if (y->balance < 0) {
			/*
			 * Left-right case.
			 *
			 *         x
			 *        / \            z
			 *       y   D          / \
			 *      / \     -->    y   x
			 *     A   z          /|   |\
			 *        / \        A B   C D
			 *       B   C
			 */
			z = y->rlink;
			y->rlink = z->llink;
			z->llink = y;
			x->llink = z->rlink;
			z->rlink = x;
			*base = z;

			x->balance = z->balance > 0 ? -1 : 0;
			y->balance = z->balance < 0 ? 1 : 0;
			z->balance = 0;
		} else {
			/*
			 * Left-left case.
			 *
			 *        x           y
			 *       / \         / \
			 *      y   C  -->  A   x
			 *     / \             / \
			 *    A   B           B   C
			 */
			x->llink = y->rlink;
			y->rlink = x;
			*base = y;

			x->balance = 0;
			y->balance = 0;
		}
	} else if (x->balance < -1) {
		y = x->rlink;
		if (y->balance > 0) {
			/*
			 * Right-left case.
			 *
			 *       x
			 *      / \              z
			 *     A   y            / \
			 *        / \   -->    x   y
			 *       z   D        /|   |\
			 *      / \          A B   C D
			 *     B   C
			 */
			node_t *z = y->llink;
			x->rlink = z->llink;
			z->llink = x;
			y->llink = z->rlink;
			z->rlink = y;
			*base = z;

			x->balance = z->balance < 0 ? 1 : 0;
			y->balance = z->balance > 0 ? -1 : 0;
			z->balance = 0;
		} else {
			/*
			 * Right-right case.
			 *
			 *       x               y
			 *      / \             / \
			 *     A   y    -->    x   C
			 *        / \         / \
			 *       B   C       A   B
			 */
			x->rlink = y->llink;
			y->llink = x;
			*base = y;

			x->balance = 0;
			y->balance = 0;
		}
	}

	/* Return the new entry. */
	*rootp = root;
	return (&result->key);
}
