/* $FreeBSD: head/contrib/gcc/config/aarch64/freebsd.h 280437 2015-03-24 14:22:58Z andrew $ */

#undef INIT_SECTION_ASM_OP
#undef FINI_SECTION_ASM_OP
#define INIT_ARRAY_SECTION_ASM_OP "\t.section\t.init_array,\"aw\",%init_array"
#define FINI_ARRAY_SECTION_ASM_OP "\t.section\t.fini_array,\"aw\",%fini_array"
