#	From: @(#)Makefile	8.1 (Berkeley) 6/8/93
# $FreeBSD: head/share/misc/Makefile 284255 2015-06-11 04:22:17Z sjg $

.include <src.opts.mk>

.PATH:	${.CURDIR}/../../sys/dev/usb

FILES=	ascii birthtoken bsd-family-tree committers-doc.dot committers-ports.dot \
	committers-src.dot eqnchar flowers init.ee \
	iso3166 iso639 latin1 man.template mdoc.template operator pci_vendors \
	scsi_modes \
	organization.dot

FILESDIR=	${BINDIR}/misc

.if ${MK_USB} != "no"
FILES+=	usb_hid_usages
FILES+=	usbdevs
.endif

.include <bsd.prog.mk>
