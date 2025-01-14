# $FreeBSD: head/share/mk/bsd.links.mk 298107 2016-04-16 07:45:30Z gjb $

.if !target(__<bsd.init.mk>__)
.error bsd.links.mk cannot be included directly.
.endif

.if defined(NO_ROOT)
.if !defined(TAGS) || ! ${TAGS:Mpackage=*}
TAGS+=         package=${PACKAGE}
.endif
TAG_ARGS=      -T ${TAGS:[*]:S/ /,/g}
.endif

afterinstall: _installlinks
.ORDER: realinstall _installlinks
_installlinks:
.for s t in ${LINKS}
	@${ECHO} "${t} -> ${s}" ;\
	${INSTALL_LINK} ${TAG_ARGS} ${DESTDIR}${s} ${DESTDIR}${t}
.endfor
.for s t in ${SYMLINKS}
	@${ECHO} "${t} -> ${s}" ;\
	${INSTALL_SYMLINK} ${TAG_ARGS} ${s} ${DESTDIR}/${t}
.endfor
