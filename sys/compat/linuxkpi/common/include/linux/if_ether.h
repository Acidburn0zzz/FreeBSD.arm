/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013 Mellanox Technologies, Ltd.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: head/sys/compat/linuxkpi/common/include/linux/if_ether.h 290135 2015-10-29 08:28:39Z hselasky $
 */
#ifndef	_LINUX_IF_ETHER_H_
#define	_LINUX_IF_ETHER_H_

#include <linux/types.h>

#include <net/ethernet.h>

#define	ETH_P_8021Q	ETHERTYPE_VLAN

#define ETH_HLEN        ETHER_HDR_LEN   /* Total octets in header.                              */
#ifndef ETH_ALEN
#define ETH_ALEN        ETHER_ADDR_LEN
#endif
#define ETH_FCS_LEN     4               /* Octets in the FCS                                    */
#define VLAN_HLEN       4               /* The additional bytes (on top of the Ethernet header)
                                         * that VLAN requires.                                  */
/*
 * defined Ethernet Protocol ID's.
 */
#define ETH_P_IP        0x0800          /* Internet Protocol packet                             */

#endif	/* _LINUX_IF_ETHER_H_ */
