# $FreeBSD: head/sys/modules/random_other/Makefile 286839 2015-08-17 07:36:12Z markm $

.PATH:	${.CURDIR}/../../dev/random

KMOD	= random_OTHER
SRCS	= randomdev.c hash.c other_algorithm.c
SRCS	+= opt_param.h bus_if.h device_if.h
SRCS	+= opt_ddb.h
CFLAGS	+= -DRANDOM_LOADABLE

.include <bsd.kmod.mk>
