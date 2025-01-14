#	from: @(#)bsd.lib.mk	5.26 (Berkeley) 5/2/91
# $FreeBSD: head/share/mk/bsd.lib.mk 301933 2016-06-15 23:57:32Z bdrewery $
#

.include <bsd.init.mk>

.if defined(LIB_CXX) || defined(SHLIB_CXX)
_LD=	${CXX}
.else
_LD=	${CC}
.endif
.if defined(LIB_CXX)
LIB=	${LIB_CXX}
.endif
.if defined(SHLIB_CXX)
SHLIB=	${SHLIB_CXX}
.endif

LIB_PRIVATE=	${PRIVATELIB:Dprivate}
# Set up the variables controlling shared libraries.  After this section,
# SHLIB_NAME will be defined only if we are to create a shared library.
# SHLIB_LINK will be defined only if we are to create a link to it.
# INSTALL_PIC_ARCHIVE will be defined only if we are to create a PIC archive.
.if defined(NO_PIC)
.undef SHLIB_NAME
.undef INSTALL_PIC_ARCHIVE
.else
.if !defined(SHLIB) && defined(LIB)
SHLIB=		${LIB}
.endif
.if !defined(SHLIB_NAME) && defined(SHLIB) && defined(SHLIB_MAJOR)
SHLIB_NAME=	lib${LIB_PRIVATE}${SHLIB}.so.${SHLIB_MAJOR}
.endif
.if defined(SHLIB_NAME) && !empty(SHLIB_NAME:M*.so.*)
SHLIB_LINK?=	${SHLIB_NAME:R}
.endif
SONAME?=	${SHLIB_NAME}
.endif

.if defined(CRUNCH_CFLAGS)
CFLAGS+=	${CRUNCH_CFLAGS}
.endif

.if ${MK_ASSERT_DEBUG} == "no"
CFLAGS+= -DNDEBUG
NO_WERROR=
.endif

.if defined(DEBUG_FLAGS)
CFLAGS+= ${DEBUG_FLAGS}

.if ${MK_CTF} != "no" && ${DEBUG_FLAGS:M-g} != ""
CTFFLAGS+= -g
.endif
.else
STRIP?=	-s
.endif

.if ${SHLIBDIR:M*lib32*}
TAGS+=	lib32
.endif

.if defined(NO_ROOT)
.if !defined(TAGS) || ! ${TAGS:Mpackage=*}
TAGS+=		package=${PACKAGE:Uruntime}
.endif
TAG_ARGS=	-T ${TAGS:[*]:S/ /,/g}
.endif

.if ${MK_DEBUG_FILES} != "no" && empty(DEBUG_FLAGS:M-g) && \
    empty(DEBUG_FLAGS:M-gdwarf*)
SHARED_CFLAGS+= -g
SHARED_CXXFLAGS+= -g
CTFFLAGS+= -g
.endif

.include <bsd.libnames.mk>

# prefer .s to a .c, add .po, remove stuff not used in the BSD libraries
# .So used for PIC object files
.SUFFIXES:
.SUFFIXES: .out .o .po .So .S .asm .s .c .cc .cpp .cxx .C .f .y .l .ln

.if !defined(PICFLAG)
.if ${MACHINE_CPUARCH} == "sparc64"
PICFLAG=-fPIC
.else
PICFLAG=-fpic
.endif
.endif

PO_FLAG=-pg

.c.o:
	${CC} ${STATIC_CFLAGS} ${CFLAGS} -c ${.IMPSRC} -o ${.TARGET}
	${CTFCONVERT_CMD}

.c.po:
	${CC} ${PO_FLAG} ${STATIC_CFLAGS} ${PO_CFLAGS} -c ${.IMPSRC} -o ${.TARGET}
	${CTFCONVERT_CMD}

.c.So:
	${CC} ${PICFLAG} -DPIC ${SHARED_CFLAGS} ${CFLAGS} -c ${.IMPSRC} -o ${.TARGET}
	${CTFCONVERT_CMD}

.cc.o .C.o .cpp.o .cxx.o:
	${CXX} ${STATIC_CXXFLAGS} ${CXXFLAGS} -c ${.IMPSRC} -o ${.TARGET}

.cc.po .C.po .cpp.po .cxx.po:
	${CXX} ${PO_FLAG} ${STATIC_CXXFLAGS} ${PO_CXXFLAGS} -c ${.IMPSRC} -o ${.TARGET}

.cc.So .C.So .cpp.So .cxx.So:
	${CXX} ${PICFLAG} -DPIC ${SHARED_CXXFLAGS} ${CXXFLAGS} -c ${.IMPSRC} -o ${.TARGET}

.f.po:
	${FC} -pg ${FFLAGS} -o ${.TARGET} -c ${.IMPSRC}
	${CTFCONVERT_CMD}

.f.So:
	${FC} ${PICFLAG} -DPIC ${FFLAGS} -o ${.TARGET} -c ${.IMPSRC}
	${CTFCONVERT_CMD}

.s.po .s.So:
	${AS} ${AFLAGS} -o ${.TARGET} ${.IMPSRC}
	${CTFCONVERT_CMD}

.asm.po:
	${CC:N${CCACHE_BIN}} -x assembler-with-cpp -DPROF ${PO_CFLAGS} \
	    ${ACFLAGS} -c ${.IMPSRC} -o ${.TARGET}
	${CTFCONVERT_CMD}

.asm.So:
	${CC:N${CCACHE_BIN}} -x assembler-with-cpp ${PICFLAG} -DPIC \
	    ${CFLAGS} ${ACFLAGS} -c ${.IMPSRC} -o ${.TARGET}
	${CTFCONVERT_CMD}

.S.po:
	${CC:N${CCACHE_BIN}} -DPROF ${PO_CFLAGS} ${ACFLAGS} -c ${.IMPSRC} \
	    -o ${.TARGET}
	${CTFCONVERT_CMD}

.S.So:
	${CC:N${CCACHE_BIN}} ${PICFLAG} -DPIC ${CFLAGS} ${ACFLAGS} \
	    -c ${.IMPSRC} -o ${.TARGET}
	${CTFCONVERT_CMD}

_LIBDIR:=${LIBDIR}
_SHLIBDIR:=${SHLIBDIR}

.if defined(SHLIB_NAME)
.if ${MK_DEBUG_FILES} != "no"
SHLIB_NAME_FULL=${SHLIB_NAME}.full
# Use ${DEBUGDIR} for base system debug files, else .debug subdirectory
.if ${_SHLIBDIR} == "/boot" ||\
    ${SHLIBDIR:C%/lib(/.*)?$%/lib%} == "/lib" ||\
    ${SHLIBDIR:C%/usr/(tests/)?lib(32|exec)?(/.*)?%/usr/lib%} == "/usr/lib"
DEBUGFILEDIR=${DEBUGDIR}${_SHLIBDIR}
.else
DEBUGFILEDIR=${_SHLIBDIR}/.debug
.endif
.if !exists(${DESTDIR}${DEBUGFILEDIR})
DEBUGMKDIR=
.endif
.else
SHLIB_NAME_FULL=${SHLIB_NAME}
.endif
.endif

.include <bsd.symver.mk>

# Allow libraries to specify their own version map or have it
# automatically generated (see bsd.symver.mk above).
.if ${MK_SYMVER} == "yes" && !empty(VERSION_MAP)
${SHLIB_NAME_FULL}:	${VERSION_MAP}
LDFLAGS+=	-Wl,--version-script=${VERSION_MAP}
.endif

.if defined(LIB) && !empty(LIB) || defined(SHLIB_NAME)
OBJS+=		${SRCS:N*.h:R:S/$/.o/}
CLEANFILES+=	${OBJS} ${STATICOBJS}
.endif

.if defined(LIB) && !empty(LIB)
_LIBS=		lib${LIB_PRIVATE}${LIB}.a

lib${LIB_PRIVATE}${LIB}.a: ${OBJS} ${STATICOBJS}
	@${ECHO} building static ${LIB} library
	@rm -f ${.TARGET}
	${AR} ${ARFLAGS} ${.TARGET} `NM='${NM}' NMFLAGS='${NMFLAGS}' lorder ${OBJS} ${STATICOBJS} | tsort -q` ${ARADD}
	${RANLIB} ${RANLIBFLAGS} ${.TARGET}
.endif

.if !defined(INTERNALLIB)

.if ${MK_PROFILE} != "no" && defined(LIB) && !empty(LIB)
_LIBS+=		lib${LIB_PRIVATE}${LIB}_p.a
POBJS+=		${OBJS:.o=.po} ${STATICOBJS:.o=.po}
DEPENDOBJS+=	${POBJS}
CLEANFILES+=	${POBJS}

lib${LIB_PRIVATE}${LIB}_p.a: ${POBJS}
	@${ECHO} building profiled ${LIB} library
	@rm -f ${.TARGET}
	${AR} ${ARFLAGS} ${.TARGET} `NM='${NM}' NMFLAGS='${NMFLAGS}' lorder ${POBJS} | tsort -q` ${ARADD}
	${RANLIB} ${RANLIBFLAGS} ${.TARGET}
.endif

.if defined(SHLIB_NAME) || \
    defined(INSTALL_PIC_ARCHIVE) && defined(LIB) && !empty(LIB)
SOBJS+=		${OBJS:.o=.So}
DEPENDOBJS+=	${SOBJS}
CLEANFILES+=	${SOBJS}
.endif

.if defined(SHLIB_NAME)
_LIBS+=		${SHLIB_NAME}

SOLINKOPTS+=	-shared -Wl,-x
.if defined(LD_FATAL_WARNINGS) && ${LD_FATAL_WARNINGS} == "no"
SOLINKOPTS+=	-Wl,--no-fatal-warnings
.else
SOLINKOPTS+=	-Wl,--fatal-warnings
.endif
SOLINKOPTS+=	-Wl,--warn-shared-textrel

.if target(beforelinking)
beforelinking: ${SOBJS}
${SHLIB_NAME_FULL}: beforelinking
.endif

.if defined(SHLIB_LINK)
.if defined(SHLIB_LDSCRIPT) && !empty(SHLIB_LDSCRIPT) && exists(${.CURDIR}/${SHLIB_LDSCRIPT})
${SHLIB_LINK:R}.ld: ${.CURDIR}/${SHLIB_LDSCRIPT}
	sed -e 's,@@SHLIB@@,${_SHLIBDIR}/${SHLIB_NAME},g' \
	    -e 's,@@LIBDIR@@,${_LIBDIR},g' \
	    ${.ALLSRC} > ${.TARGET}

${SHLIB_NAME_FULL}: ${SHLIB_LINK:R}.ld
CLEANFILES+=	${SHLIB_LINK:R}.ld
.endif
CLEANFILES+=	${SHLIB_LINK}
.endif

${SHLIB_NAME_FULL}: ${SOBJS}
	@${ECHO} building shared library ${SHLIB_NAME}
	@rm -f ${SHLIB_NAME} ${SHLIB_LINK}
.if defined(SHLIB_LINK) && !commands(${SHLIB_LINK:R}.ld) && ${MK_DEBUG_FILES} == "no"
	@${INSTALL_SYMLINK} ${TAG_ARGS:D${TAG_ARGS},development} ${SHLIB_NAME} ${SHLIB_LINK}
.endif
	${_LD:N${CCACHE_BIN}} ${LDFLAGS} ${SSP_CFLAGS} ${SOLINKOPTS} \
	    -o ${.TARGET} -Wl,-soname,${SONAME} \
	    `NM='${NM}' NMFLAGS='${NMFLAGS}' lorder ${SOBJS} | tsort -q` ${LDADD}
.if ${MK_CTF} != "no"
	${CTFMERGE} ${CTFFLAGS} -o ${.TARGET} ${SOBJS}
.endif

.if ${MK_DEBUG_FILES} != "no"
CLEANFILES+=	${SHLIB_NAME_FULL} ${SHLIB_NAME}.debug
${SHLIB_NAME}: ${SHLIB_NAME_FULL} ${SHLIB_NAME}.debug
	${OBJCOPY} --strip-debug --add-gnu-debuglink=${SHLIB_NAME}.debug \
	    ${SHLIB_NAME_FULL} ${.TARGET}
.if defined(SHLIB_LINK) && !commands(${SHLIB_LINK:R}.ld)
	@${INSTALL_SYMLINK} ${TAG_ARGS:D${TAG_ARGS},development} ${SHLIB_NAME} ${SHLIB_LINK}
.endif

${SHLIB_NAME}.debug: ${SHLIB_NAME_FULL}
	${OBJCOPY} --only-keep-debug ${SHLIB_NAME_FULL} ${.TARGET}
.endif
.endif #defined(SHLIB_NAME)

.if defined(INSTALL_PIC_ARCHIVE) && defined(LIB) && !empty(LIB) && ${MK_TOOLCHAIN} != "no"
_LIBS+=		lib${LIB_PRIVATE}${LIB}_pic.a

lib${LIB_PRIVATE}${LIB}_pic.a: ${SOBJS}
	@${ECHO} building special pic ${LIB} library
	@rm -f ${.TARGET}
	${AR} ${ARFLAGS} ${.TARGET} ${SOBJS} ${ARADD}
	${RANLIB} ${RANLIBFLAGS} ${.TARGET}
.endif

.if defined(WANT_LINT) && !defined(NO_LINT) && defined(LIB) && !empty(LIB)
LINTLIB=	llib-l${LIB}.ln
_LIBS+=		${LINTLIB}
LINTOBJS+=	${SRCS:M*.c:.c=.ln}
CLEANFILES+=	${LINTOBJS}

${LINTLIB}: ${LINTOBJS}
	@${ECHO} building lint library ${.TARGET}
	@rm -f ${.TARGET}
	${LINT} ${LINTLIBFLAGS} ${CFLAGS:M-[DIU]*} ${.ALLSRC}
.endif

.endif # !defined(INTERNALLIB)

.if defined(_SKIP_BUILD)
all:
.else
.if defined(_LIBS) && !empty(_LIBS)
all: ${_LIBS}
CLEANFILES+=	${_LIBS}
.endif

.if ${MK_MAN} != "no" && !defined(LIBRARIES_ONLY)
all: all-man
.endif
.endif

_EXTRADEPEND:
.if !defined(NO_EXTRADEPEND) && defined(SHLIB_NAME)
.if defined(DPADD) && !empty(DPADD)
	echo ${SHLIB_NAME_FULL}: ${DPADD} >> ${DEPENDFILE}
.endif
.endif

.if !target(install)

.if defined(PRECIOUSLIB)
.if !defined(NO_FSCHG)
SHLINSTALLFLAGS+= -fschg
.endif
SHLINSTALLFLAGS+= -S
.endif

_INSTALLFLAGS:=	${INSTALLFLAGS}
.for ie in ${INSTALLFLAGS_EDIT}
_INSTALLFLAGS:=	${_INSTALLFLAGS${ie}}
.endfor
_SHLINSTALLFLAGS:=	${SHLINSTALLFLAGS}
.for ie in ${INSTALLFLAGS_EDIT}
_SHLINSTALLFLAGS:=	${_SHLINSTALLFLAGS${ie}}
.endfor

.if !defined(INTERNALLIB)
realinstall: _libinstall
.ORDER: beforeinstall _libinstall
_libinstall:
.if defined(LIB) && !empty(LIB) && ${MK_INSTALLLIB} != "no"
	${INSTALL} ${TAG_ARGS:D${TAG_ARGS},development} -C -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${_INSTALLFLAGS} lib${LIB_PRIVATE}${LIB}.a ${DESTDIR}${_LIBDIR}/
.endif
.if ${MK_PROFILE} != "no" && defined(LIB) && !empty(LIB)
	${INSTALL} ${TAG_ARGS:D${TAG_ARGS},profile} -C -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${_INSTALLFLAGS} lib${LIB_PRIVATE}${LIB}_p.a ${DESTDIR}${_LIBDIR}/
.endif
.if defined(SHLIB_NAME)
	${INSTALL} ${TAG_ARGS} ${STRIP} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${_INSTALLFLAGS} ${_SHLINSTALLFLAGS} \
	    ${SHLIB_NAME} ${DESTDIR}${_SHLIBDIR}/
.if ${MK_DEBUG_FILES} != "no"
.if defined(DEBUGMKDIR)
	${INSTALL} ${TAG_ARGS:D${TAG_ARGS},debug} -d ${DESTDIR}${DEBUGFILEDIR}/
.endif
	${INSTALL} ${TAG_ARGS:D${TAG_ARGS},debug} -o ${LIBOWN} -g ${LIBGRP} -m ${DEBUGMODE} \
	    ${_INSTALLFLAGS} \
	    ${SHLIB_NAME}.debug ${DESTDIR}${DEBUGFILEDIR}/
.endif
.if defined(SHLIB_LINK)
.if commands(${SHLIB_LINK:R}.ld)
	${INSTALL} ${TAG_ARGS:D${TAG_ARGS},development} -S -C -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${_INSTALLFLAGS} ${SHLIB_LINK:R}.ld \
	    ${DESTDIR}${_LIBDIR}/${SHLIB_LINK}
.for _SHLIB_LINK_LINK in ${SHLIB_LDSCRIPT_LINKS}
	${INSTALL_SYMLINK} ${SHLIB_LINK} ${DESTDIR}${_LIBDIR}/${_SHLIB_LINK_LINK}
.endfor
.else
.if ${_SHLIBDIR} == ${_LIBDIR}
.if ${SHLIB_LINK:Mlib*}
	${INSTALL_RSYMLINK} ${TAG_ARGS:D${TAG_ARGS},development} ${SHLIB_NAME} ${DESTDIR}${_LIBDIR}/${SHLIB_LINK}
.else
	${INSTALL_RSYMLINK} ${TAG_ARGS} ${DESTDIR}${_SHLIBDIR}/${SHLIB_NAME} \
	    ${DESTDIR}${_LIBDIR}/${SHLIB_LINK}
.endif
.else
.if ${SHLIB_LINK:Mlib*}
	${INSTALL_RSYMLINK} ${TAG_ARGS:D${TAG_ARGS},development} ${DESTDIR}${_SHLIBDIR}/${SHLIB_NAME} \
	    ${DESTDIR}${_LIBDIR}/${SHLIB_LINK}
.else
	${INSTALL_RSYMLINK} ${TAG_ARGS} ${DESTDIR}${_SHLIBDIR}/${SHLIB_NAME} \
	    ${DESTDIR}${_LIBDIR}/${SHLIB_LINK}
.endif
.if exists(${DESTDIR}${_LIBDIR}/${SHLIB_NAME})
	-chflags noschg ${DESTDIR}${_LIBDIR}/${SHLIB_NAME}
	rm -f ${DESTDIR}${_LIBDIR}/${SHLIB_NAME}
.endif
.endif
.endif # SHLIB_LDSCRIPT
.endif # SHLIB_LINK
.endif # SHIB_NAME
.if defined(INSTALL_PIC_ARCHIVE) && defined(LIB) && !empty(LIB) && ${MK_TOOLCHAIN} != "no"
	${INSTALL} ${TAG_ARGS:D${TAG_ARGS},development} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${_INSTALLFLAGS} lib${LIB}_pic.a ${DESTDIR}${_LIBDIR}/
.endif
.if defined(WANT_LINT) && !defined(NO_LINT) && defined(LIB) && !empty(LIB)
	${INSTALL} ${TAG_ARGS:D${TAG_ARGS},development} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${_INSTALLFLAGS} ${LINTLIB} ${DESTDIR}${LINTLIBDIR}/
.endif
.endif # !defined(INTERNALLIB)

.if !defined(LIBRARIES_ONLY)
.include <bsd.nls.mk>
.include <bsd.files.mk>
.include <bsd.incs.mk>
.include <bsd.confs.mk>
.endif

.include <bsd.links.mk>

.if ${MK_MAN} != "no" && !defined(LIBRARIES_ONLY)
realinstall: maninstall
.ORDER: beforeinstall maninstall
.endif

.endif

.if !target(lint)
lint: ${SRCS:M*.c}
	${LINT} ${LINTFLAGS} ${CFLAGS:M-[DIU]*} ${.ALLSRC}
.endif

.if ${MK_MAN} != "no" && !defined(LIBRARIES_ONLY)
.include <bsd.man.mk>
.endif

.if defined(LIB) && !empty(LIB)
OBJS_DEPEND_GUESS+= ${SRCS:M*.h}
.for _S in ${SRCS:N*.[hly]}
OBJS_DEPEND_GUESS.${_S:R}.po+=	${_S}
.endfor
.endif
.if defined(SHLIB_NAME) || \
    defined(INSTALL_PIC_ARCHIVE) && defined(LIB) && !empty(LIB)
.for _S in ${SRCS:N*.[hly]}
OBJS_DEPEND_GUESS.${_S:R}.So+=	${_S}
.endfor
.endif

.include <bsd.dep.mk>
.include <bsd.clang-analyze.mk>
.include <bsd.obj.mk>
.include <bsd.sys.mk>
