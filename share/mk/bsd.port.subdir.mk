# $FreeBSD: head/share/mk/bsd.port.subdir.mk 287436 2015-09-03 17:01:58Z bdrewery $

.if !defined(PORTSDIR)
# Autodetect if the command is being run in a ports tree that's not rooted
# in the default /usr/ports.  The ../../.. case is in case ports ever grows
# a third level.
.for RELPATH in . .. ../.. ../../..
.if !defined(_PORTSDIR) && exists(${.CURDIR}/${RELPATH}/Mk/bsd.port.mk)
_PORTSDIR=	${.CURDIR}/${RELPATH}
.endif
.endfor
_PORTSDIR?=	/usr/ports
.if defined(.PARSEDIR)
PORTSDIR=	${_PORTSDIR:tA}
.else # fmake doesn't have :tA
PORTSDIR!=	realpath ${_PORTSDIR}
.endif
.endif

BSDPORTSUBDIRMK?=	${PORTSDIR}/Mk/bsd.port.subdir.mk

.include "${BSDPORTSUBDIRMK}"
