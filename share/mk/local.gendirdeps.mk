# $FreeBSD: head/share/mk/local.gendirdeps.mk 299472 2016-05-11 17:40:51Z bdrewery $

# we need a keyword, this noise is to prevent it being expanded
GENDIRDEPS_HEADER= echo '\# ${FreeBSD:L:@v@$$$v$$ @:M*F*}';

# suppress optional/auto dependencies
# local.dirdeps.mk will put them in if necessary
GENDIRDEPS_FILTER+= \
	Nbin/cat.host \
	Ngnu/lib/libssp/libssp_nonshared \
	Ncddl/usr.bin/ctf* \
	Nlib/libc_nonshared \
	Ntargets/pseudo/stage* \
	Ntools/*

# Exclude toolchain which is handled special.
.if ${RELDIR:Mtargets*} == ""
.if ${RELDIR:Nusr.bin/clang/*:Ngnu/usr.bin/cc/*:Nlib/clang*} != ""
GENDIRDEPS_FILTER.host+= \
	Nusr.bin/clang/* \
	Ngnu/usr.bin/cc/* \

.endif
GENDIRDEPS_FILTER_HOST_TOOLS+= \
	Nlib/clang/include \
	Nusr.bin/addr2line \
	Nusr.bin/ar \
	Nusr.bin/clang/clang \
	Nusr.bin/elfcopy \
	Nusr.bin/elfdump \
	Nusr.bin/nm \
	Nusr.bin/readelf \
	Nusr.bin/size \
	Nusr.bin/strings \
	Nusr.bin/strip \
	Ngnu/usr.bin/cc* \
	Ngnu/usr.bin/binutils* \

.if ${MACHINE} != "host"
GENDIRDEPS_FILTER+=	${GENDIRDEPS_FILTER_HOST_TOOLS:C,$,.host,}
.else
GENDIRDEPS_FILTER+=	${GENDIRDEPS_FILTER_HOST_TOOLS}
.endif
.endif

GENDIRDEPS_FILTER+= ${GENDIRDEPS_FILTER.${MACHINE}:U}

# gendirdeps.mk will turn _{VAR} into ${VAR} which keeps this simple
# order of this list matters!
GENDIRDEPS_FILTER_DIR_VARS+= \
       CSU_DIR \
       BOOT_MACHINE_DIR

# order of this list matters!
GENDIRDEPS_FILTER_VARS+= \
       KERNEL_NAME \
       MACHINE_CPUARCH \
       MACHINE_ARCH \
       MACHINE

GENDIRDEPS_FILTER+= ${GENDIRDEPS_FILTER_DIR_VARS:@v@S,${$v},_{${v}},@}
GENDIRDEPS_FILTER+= ${GENDIRDEPS_FILTER_VARS:@v@S,/${$v}/,/_{${v}}/,@:NS,//,*:u}
