# $FreeBSD: head/share/mk/bsd.dep.mk 302073 2016-06-21 21:55:03Z bdrewery $
#
# The include file <bsd.dep.mk> handles Makefile dependencies.
#
#
# +++ variables +++
#
# CLEANDEPENDDIRS	Additional directories to remove for the cleandepend
# 			target.
#
# CLEANDEPENDFILES	Additional files to remove for the cleandepend target.
#
# CTAGS		A tags file generation program [gtags]
#
# CTAGSFLAGS	Options for ctags(1) [not set]
#
# DEPENDFILE	dependencies file [.depend]
#
# GTAGSFLAGS	Options for gtags(1) [-o]
#
# HTAGSFLAGS	Options for htags(1) [not set]
#
# SRCS          List of source files (c, c++, assembler)
#
# DPSRCS	List of source files which are needed for generating
#		dependencies, ${SRCS} are always part of it.
#
# +++ targets +++
#
#	cleandepend:
#		remove ${CLEANDEPENDFILES}; remove ${CLEANDEPENDDIRS} and all
#		contents.
#
#	depend:
#		Make the dependencies for the source files, and store
#		them in the file ${DEPENDFILE}.
#
#	tags:
#		In "ctags" mode, create a tags file for the source files.
#		In "gtags" mode, create a (GLOBAL) gtags file for the
#		source files.  If HTML is defined, htags(1) is also run
#		after gtags(1).

.if !target(__<bsd.init.mk>__)
.error bsd.dep.mk cannot be included directly.
.endif

CTAGS?=		gtags
CTAGSFLAGS?=
GTAGSFLAGS?=	-o
HTAGSFLAGS?=

.if ${MK_DIRDEPS_BUILD} == "no"
.MAKE.DEPENDFILE= ${DEPENDFILE}
.endif
CLEANDEPENDFILES+=	${DEPENDFILE} ${DEPENDFILE}.*
.if ${MK_META_MODE} == "yes"
CLEANDEPENDFILES+=	*.meta
.endif

# Keep `tags' here, before SRCS are mangled below for `depend'.
.if !target(tags) && defined(SRCS) && !defined(NO_TAGS)
tags: ${SRCS}
.if ${CTAGS:T} == "gtags"
	@cd ${.CURDIR} && ${CTAGS} ${GTAGSFLAGS} ${.OBJDIR}
.if defined(HTML)
	@cd ${.CURDIR} && htags ${HTAGSFLAGS} -d ${.OBJDIR} ${.OBJDIR}
.endif
.else
	@${CTAGS} ${CTAGSFLAGS} -f /dev/stdout \
	    ${.ALLSRC:N*.h} | sed "s;${.CURDIR}/;;" > ${.TARGET}
.endif
.endif

.if !empty(.MAKE.MODE:Mmeta) && empty(.MAKE.MODE:Mnofilemon)
_meta_filemon=	1
.endif

# Skip reading .depend when not needed to speed up tree-walks
# and simple lookups.
# Also skip generating or including .depend.* files if in meta+filemon mode
# since it will track dependencies itself.  OBJS_DEPEND_GUESS is still used.
.if !empty(.MAKEFLAGS:M-V${_V_READ_DEPEND}) || make(obj) || make(clean*) || \
    make(install*) || make(analyze) || defined(_meta_filemon)
_SKIP_READ_DEPEND=	1
.if ${MK_DIRDEPS_BUILD} == "no"
.MAKE.DEPENDFILE=	/dev/null
.endif
.endif

.if defined(SRCS)
CLEANFILES?=

.for _S in ${SRCS:N*.[dhly]}
OBJS_DEPEND_GUESS.${_S:R}.o+=	${_S}
.endfor

# Lexical analyzers
.for _LSRC in ${SRCS:M*.l:N*/*}
.for _LC in ${_LSRC:R}.c
${_LC}: ${_LSRC}
	${LEX} ${LFLAGS} -o${.TARGET} ${.ALLSRC}
OBJS_DEPEND_GUESS.${_LC:R}.o+=	${_LC}
SRCS:=	${SRCS:S/${_LSRC}/${_LC}/}
CLEANFILES+= ${_LC}
.endfor
.endfor

# Yacc grammars
.for _YSRC in ${SRCS:M*.y:N*/*}
.for _YC in ${_YSRC:R}.c
SRCS:=	${SRCS:S/${_YSRC}/${_YC}/}
CLEANFILES+= ${_YC}
.if !empty(YFLAGS:M-d) && !empty(SRCS:My.tab.h)
.ORDER: ${_YC} y.tab.h
y.tab.h: .NOMETA
${_YC} y.tab.h: ${_YSRC}
	${YACC} ${YFLAGS} ${.ALLSRC}
	cp y.tab.c ${_YC}
CLEANFILES+= y.tab.c y.tab.h
.elif !empty(YFLAGS:M-d)
.for _YH in ${_YC:R}.h
.ORDER: ${_YC} ${_YH}
${_YH}: .NOMETA
${_YC} ${_YH}: ${_YSRC}
	${YACC} ${YFLAGS} -o ${_YC} ${.ALLSRC}
SRCS+=	${_YH}
CLEANFILES+= ${_YH}
.endfor
.else
${_YC}: ${_YSRC}
	${YACC} ${YFLAGS} -o ${_YC} ${.ALLSRC}
.endif
OBJS_DEPEND_GUESS.${_YC:R}.o+=	${_YC}
.endfor
.endfor

# DTrace probe definitions
.if ${SRCS:M*.d}
CFLAGS+=	-I${.OBJDIR}
.endif
.for _DSRC in ${SRCS:M*.d:N*/*}
.for _D in ${_DSRC:R}
SRCS+=	${_D}.h
${_D}.h: ${_DSRC}
	${DTRACE} ${DTRACEFLAGS} -h -s ${.ALLSRC}
SRCS:=	${SRCS:S/^${_DSRC}$//}
OBJS+=	${_D}.o
CLEANFILES+= ${_D}.h ${_D}.o
${_D}.o: ${_DSRC} ${OBJS:S/^${_D}.o$//}
	@rm -f ${.TARGET}
	${DTRACE} ${DTRACEFLAGS} -G -o ${.TARGET} -s ${.ALLSRC:N*.h}
.if defined(LIB)
CLEANFILES+= ${_D}.So ${_D}.po
${_D}.So: ${_DSRC} ${SOBJS:S/^${_D}.So$//}
	@rm -f ${.TARGET}
	${DTRACE} ${DTRACEFLAGS} -G -o ${.TARGET} -s ${.ALLSRC:N*.h}
${_D}.po: ${_DSRC} ${POBJS:S/^${_D}.po$//}
	@rm -f ${.TARGET}
	${DTRACE} ${DTRACEFLAGS} -G -o ${.TARGET} -s ${.ALLSRC:N*.h}
.endif
.endfor
.endfor


.if ${MAKE_VERSION} < 20160220
DEPEND_MP?=	-MP
.endif
# Handle OBJS=../somefile.o hacks.  Just replace '/' rather than use :T to
# avoid collisions.
DEPEND_FILTER=	C,/,_,g
DEPENDSRCS=	${SRCS:M*.[cSC]} ${SRCS:M*.cxx} ${SRCS:M*.cpp} ${SRCS:M*.cc}
.if !empty(DEPENDSRCS)
DEPENDOBJS+=	${DEPENDSRCS:R:S,$,.o,}
.endif
DEPENDFILES_OBJS=	${DEPENDOBJS:O:u:${DEPEND_FILTER}:C/^/${DEPENDFILE}./}
DEPEND_CFLAGS+=	-MD ${DEPEND_MP} -MF${DEPENDFILE}.${.TARGET:${DEPEND_FILTER}}
DEPEND_CFLAGS+=	-MT${.TARGET}
.if !defined(_meta_filemon)
.if defined(.PARSEDIR)
# Only add in DEPEND_CFLAGS for CFLAGS on files we expect from DEPENDOBJS
# as those are the only ones we will include.
DEPEND_CFLAGS_CONDITION= "${DEPENDOBJS:M${.TARGET:${DEPEND_FILTER}}}" != ""
CFLAGS+=	${${DEPEND_CFLAGS_CONDITION}:?${DEPEND_CFLAGS}:}
.else
CFLAGS+=	${DEPEND_CFLAGS}
.endif
.if !defined(_SKIP_READ_DEPEND)
.for __depend_obj in ${DEPENDFILES_OBJS}
.if ${MAKE_VERSION} < 20160220
.sinclude "${.OBJDIR}/${__depend_obj}"
.else
.dinclude "${.OBJDIR}/${__depend_obj}"
.endif
.endfor
.endif	# !defined(_SKIP_READ_DEPEND)
.endif	# !defined(_meta_filemon)
.endif	# defined(SRCS)

.if ${MK_DIRDEPS_BUILD} == "yes"
# Prevent meta.autodep.mk from tracking "local dependencies".
.depend:
.include <meta.autodep.mk>
# If using filemon then _EXTRADEPEND is skipped since it is not needed.
.if defined(_meta_filemon)
# this depend: bypasses that below
# the dependency helps when bootstrapping
depend: beforedepend ${DPSRCS} ${SRCS} afterdepend
beforedepend:
afterdepend: beforedepend
.endif
.endif

# Guess some dependencies for when no ${DEPENDFILE}.OBJ is generated yet.
# For meta+filemon the .meta file is checked for since it is the dependency
# file used.
.for __obj in ${DEPENDOBJS:O:u}
.if (defined(_meta_filemon) && !exists(${.OBJDIR}/${__obj}.meta)) || \
    (!defined(_meta_filemon) && !exists(${.OBJDIR}/${DEPENDFILE}.${__obj}))
${__obj}: ${OBJS_DEPEND_GUESS}
${__obj}: ${OBJS_DEPEND_GUESS.${__obj}}
.elif defined(_meta_filemon)
# For meta mode we still need to know which file to depend on to avoid
# ambiguous suffix transformation rules from .PATH.  Meta mode does not
# use .depend files.  We really only need source files, not headers since
# they are typically in SRCS/beforebuild already.  For target-specific
# guesses do include headers though since they may not be in SRCS.
${__obj}: ${OBJS_DEPEND_GUESS:N*.h}
${__obj}: ${OBJS_DEPEND_GUESS.${__obj}}
.endif
.endfor

# Always run 'make depend' to generate dependencies early and to avoid the
# need for manually running it.  The dirdeps build should only do this in
# sub-makes though since MAKELEVEL0 is for dirdeps calculations.
.if ${MK_DIRDEPS_BUILD} == "no" || ${.MAKE.LEVEL} > 0
beforebuild: depend
.endif

.if !target(depend)
.if defined(SRCS)
depend: beforedepend ${DEPENDFILE} afterdepend

# Tell bmake not to look for generated files via .PATH
.NOPATH: ${DEPENDFILE} ${DEPENDFILES_OBJS}

DPSRCS+= ${SRCS}
# A .depend file will only be generated if there are commands in
# beforedepend/_EXTRADEPEND/afterdepend  The _EXTRADEPEND target is
# ignored if using meta+filemon since it handles all dependencies.  The other
# targets are kept as they be used for generating something.  The target is
# kept to allow 'make depend' to generate files.
${DEPENDFILE}: ${DPSRCS}
.if exists(${.OBJDIR}/${DEPENDFILE}) || \
    ((commands(beforedepend) || \
    (!defined(_meta_filemon) && commands(_EXTRADEPEND)) || \
    commands(afterdepend)) && !empty(.MAKE.MODE:Mmeta))
	rm -f ${DEPENDFILE}
.endif
.if !defined(_meta_filemon) && target(_EXTRADEPEND)
_EXTRADEPEND: .USE
${DEPENDFILE}: _EXTRADEPEND
.endif

.ORDER: ${DEPENDFILE} afterdepend
.else
depend: beforedepend afterdepend
.endif
.if !target(beforedepend)
beforedepend:
.else
.ORDER: beforedepend ${DEPENDFILE}
.ORDER: beforedepend afterdepend
.endif
.if !target(afterdepend)
afterdepend:
.endif
.endif

.if defined(SRCS)
.if ${CTAGS:T} == "gtags"
CLEANDEPENDFILES+=	GPATH GRTAGS GSYMS GTAGS
.if defined(HTML)
CLEANDEPENDDIRS+=	HTML
.endif
.else
CLEANDEPENDFILES+=	tags
.endif
.endif
.if !target(cleandepend)
cleandepend:
.if !empty(CLEANDEPENDFILES)
	rm -f ${CLEANDEPENDFILES}
.endif
.if !empty(CLEANDEPENDDIRS)
	rm -rf ${CLEANDEPENDDIRS}
.endif
.endif

.if !target(checkdpadd) && (defined(DPADD) || defined(LDADD))
_LDADD_FROM_DPADD=	${DPADD:R:T:C;^lib(.*)$;-l\1;g}
# Ignore -Wl,--start-group/-Wl,--end-group as it might be required in the
# LDADD list due to unresolved symbols
_LDADD_CANONICALIZED=	${LDADD:N:R:T:C;^lib(.*)$;-l\1;g:N-Wl,--[es]*-group}
checkdpadd:
.if ${_LDADD_FROM_DPADD} != ${_LDADD_CANONICALIZED}
	@echo ${.CURDIR}
	@echo "DPADD -> ${_LDADD_FROM_DPADD}"
	@echo "LDADD -> ${_LDADD_CANONICALIZED}"
.endif
.endif
