# $FreeBSD: head/sys/modules/nandfs/Makefile 264551 2014-04-16 19:49:44Z imp $

.PATH: ${.CURDIR}/../../fs/nandfs

KMOD=	nandfs
SRCS=	vnode_if.h opt_ddb.h \
	bmap.c nandfs_bmap.c nandfs_dir.c nandfs_subr.c nandfs_vfsops.c \
	nandfs_vnops.c nandfs_alloc.c nandfs_cpfile.c nandfs_dat.c \
	nandfs_ifile.c nandfs_segment.c nandfs_sufile.c nandfs_buffer.c \
	nandfs_cleaner.c

.include <bsd.kmod.mk>
