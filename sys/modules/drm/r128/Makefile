# $FreeBSD: head/sys/modules/drm/r128/Makefile 276606 2015-01-03 03:35:18Z imp $

.PATH:	${.CURDIR}/../../../dev/drm
KMOD	= r128
SRCS	= r128_cce.c r128_drv.c r128_irq.c r128_state.c
SRCS	+=device_if.h bus_if.h pci_if.h opt_drm.h

.include <bsd.kmod.mk>

CWARNFLAGS.r128_cce.c=		-Wno-unused ${NO_WCONSTANT_CONVERSION}
CWARNFLAGS.r128_state.c=	-Wno-unused
