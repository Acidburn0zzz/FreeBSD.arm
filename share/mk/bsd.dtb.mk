# $FreeBSD: head/share/mk/bsd.dtb.mk 276846 2015-01-08 18:28:06Z imp $

# Search for kernel source tree in standard places.
.for _dir in ${.CURDIR}/../.. ${.CURDIR}/../../.. ${.CURDIR}/../../../.. \
    ${.CURDIR}/../../../../.. /sys /usr/src/sys
.if !defined(SYSDIR) && exists(${_dir}/kern/) && exists(${_dir}/conf/kmod.mk)
SYSDIR=	${_dir}
.endif
.endfor
.if !defined(SYSDIR) || !exists(${SYSDIR}/kern/) || \
    !exists(${SYSDIR}/conf/kmod.mk)
.error Unable to locate the kernel source tree. Set SYSDIR to override.
.endif

.include "${SYSDIR}/conf/dtb.mk"

.include <bsd.sys.mk>
