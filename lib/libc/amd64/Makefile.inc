# $FreeBSD: head/lib/libc/amd64/Makefile.inc 262722 2014-03-04 02:19:39Z marcel $
#
# Machine dependent definitions for the amd64 architecture.
#

# Long double is 80 bits
GDTOASRCS+=strtorx.c
MDSRCS+=machdep_ldisx.c
SYM_MAPS+=${LIBC_SRCTOP}/amd64/Symbol.map
