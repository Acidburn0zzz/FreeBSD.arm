# $FreeBSD: head/usr.bin/clang/llvm-symbolizer/Makefile 296417 2016-03-05 20:24:31Z dim $

.include <bsd.own.mk>

PROG_CXX=llvm-symbolizer

SRCDIR=	tools/llvm-symbolizer
SRCS=	llvm-symbolizer.cpp

LIBDEPS=llvmsymbolize \
	llvmdebuginfopdb \
	llvmdebuginfodwarf \
	llvmobject \
	llvmmcparser \
	llvmmc \
	llvmbitreader \
	llvmcore \
	llvmsupport

LIBADD+=	z

.include "../clang.prog.mk"
