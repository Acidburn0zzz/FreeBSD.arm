/*-
 * This header is BSD licensed so anyone can use the definitions to implement
 * compatible drivers/servers.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of IBM nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL IBM OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Copyright (C) Red Hat, Inc., 2009, 2010, 2011
 * Copyright (C) Amit Shah <amit.shah@redhat.com>, 2009, 2010, 2011
 *
 * $FreeBSD: head/sys/dev/virtio/console/virtio_console.h 273515 2014-10-23 04:47:32Z bryanv $
 */

#ifndef _VIRTIO_CONSOLE_H
#define _VIRTIO_CONSOLE_H

/* Feature bits */
#define VIRTIO_CONSOLE_F_SIZE		0x01	/* Console size */
#define VIRTIO_CONSOLE_F_MULTIPORT	0x02	/* Multiple ports */
#define VIRTIO_CONSOLE_F_EMERG_WRITE 	0x04 	/* Emergency write */

#define VIRTIO_CONSOLE_BAD_ID		(~(uint32_t)0)

struct virtio_console_config {
	/* colums of the screens */
	uint16_t cols;
	/* rows of the screens */
	uint16_t rows;
	/* max. number of ports this device can hold */
	uint32_t max_nr_ports;
	/* emergency write register */
	uint32_t emerg_wr;
} __packed;

/*
 * A message that's passed between the Host and the Guest for a
 * particular port.
 */
struct virtio_console_control {
	uint32_t id;		/* Port number */
	uint16_t event;		/* The kind of control event (see below) */
	uint16_t value;		/* Extra information for the key */
};

/* Some events for control messages */
#define VIRTIO_CONSOLE_DEVICE_READY	0
#define VIRTIO_CONSOLE_PORT_ADD		1
#define VIRTIO_CONSOLE_PORT_REMOVE	2
#define VIRTIO_CONSOLE_PORT_READY	3
#define VIRTIO_CONSOLE_CONSOLE_PORT	4
#define VIRTIO_CONSOLE_RESIZE		5
#define VIRTIO_CONSOLE_PORT_OPEN	6
#define VIRTIO_CONSOLE_PORT_NAME	7

#endif /* _VIRTIO_CONSOLE_H */
