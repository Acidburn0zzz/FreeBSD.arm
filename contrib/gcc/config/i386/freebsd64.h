/* Definitions for AMD x86-64 running FreeBSD with ELF format
   Copyright (C) 2002, 2004 Free Software Foundation, Inc.
   Contributed by David O'Brien <obrien@FreeBSD.org>

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GCC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING.  If not, write to
the Free Software Foundation, 51 Franklin Street, Fifth Floor,
Boston, MA 02110-1301, USA.  */

/* $FreeBSD: head/contrib/gcc/config/i386/freebsd64.h 238472 2012-07-15 10:54:10Z kib $ */


#undef  TARGET_VERSION
#define TARGET_VERSION fprintf (stderr, " (FreeBSD/amd64 ELF)");

#undef  FBSD_TARGET_CPU_CPP_BUILTINS
#define FBSD_TARGET_CPU_CPP_BUILTINS()		\
  do						\
    {						\
      if (TARGET_64BIT)				\
	{					\
	  builtin_define ("__LP64__");		\
	}					\
    }						\
  while (0)

#define SUBTARGET_EXTRA_SPECS \
  { "fbsd_dynamic_linker", FBSD_DYNAMIC_LINKER }

/* Provide a LINK_SPEC appropriate for the FreeBSD/x86-64 ELF target.
   This is a copy of LINK_SPEC from <i386/freebsd.h> tweaked for
   the x86-64 target.  */

#undef	LINK_SPEC
#define LINK_SPEC "\
  %{m32:-m elf_i386_fbsd} \
  %{v:-V} \
  %{assert*} %{R*} %{rpath*} %{defsym*} \
  %{shared:-Bshareable %{h*} %{soname*}} \
    %{!shared: \
      %{!static: \
        %{rdynamic:-export-dynamic} \
	%{!dynamic-linker:-dynamic-linker %(fbsd_dynamic_linker) }} \
    %{static:-Bstatic}} \
  %{!static:--hash-style=both --enable-new-dtags} \
  %{symbolic:-Bsymbolic}"
