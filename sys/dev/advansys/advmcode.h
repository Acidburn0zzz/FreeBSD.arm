/*-
 * Exported interface to downloadable microcode for AdvanSys SCSI Adapters
 *
 * $FreeBSD: head/sys/dev/advansys/advmcode.h 139749 2005-01-06 01:43:34Z imp $
 *
 * Obtained from:
 *
 * Copyright (c) 1995-1999 Advanced System Products, Inc.
 * All Rights Reserved.
 *   
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that redistributions of source
 * code retain the above copyright notice and this comment without
 * modification.
 */

extern u_int8_t adv_mcode[];
extern u_int16_t adv_mcode_size;
extern u_int32_t adv_mcode_chksum;
