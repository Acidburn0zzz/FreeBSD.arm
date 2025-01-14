/*-
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$KAME: frag6.c,v 1.33 2002/01/07 11:34:48 kjc Exp $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: head/sys/netinet6/frag6.c 298995 2016-05-03 18:05:43Z pfg $");

#include "opt_rss.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/eventhandler.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/netisr.h>
#include <net/route.h>
#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet/icmp6.h>
#include <netinet/in_systm.h>	/* for ECN definitions */
#include <netinet/ip.h>		/* for ECN definitions */

#include <security/mac/mac_framework.h>

static void frag6_enq(struct ip6asfrag *, struct ip6asfrag *);
static void frag6_deq(struct ip6asfrag *);
static void frag6_insque(struct ip6q *, struct ip6q *);
static void frag6_remque(struct ip6q *);
static void frag6_freef(struct ip6q *);

static struct mtx ip6qlock;
/*
 * These fields all protected by ip6qlock.
 */
static VNET_DEFINE(u_int, frag6_nfragpackets);
static VNET_DEFINE(u_int, frag6_nfrags);
static VNET_DEFINE(struct ip6q, ip6q);	/* ip6 reassemble queue */

#define	V_frag6_nfragpackets		VNET(frag6_nfragpackets)
#define	V_frag6_nfrags			VNET(frag6_nfrags)
#define	V_ip6q				VNET(ip6q)

#define	IP6Q_LOCK_INIT()	mtx_init(&ip6qlock, "ip6qlock", NULL, MTX_DEF);
#define	IP6Q_LOCK()		mtx_lock(&ip6qlock)
#define	IP6Q_TRYLOCK()		mtx_trylock(&ip6qlock)
#define	IP6Q_LOCK_ASSERT()	mtx_assert(&ip6qlock, MA_OWNED)
#define	IP6Q_UNLOCK()		mtx_unlock(&ip6qlock)

static MALLOC_DEFINE(M_FTABLE, "fragment", "fragment reassembly header");

/*
 * Initialise reassembly queue and fragment identifier.
 */
static void
frag6_change(void *tag)
{

	V_ip6_maxfragpackets = nmbclusters / 4;
	V_ip6_maxfrags = nmbclusters / 4;
}

void
frag6_init(void)
{

	V_ip6_maxfragpackets = nmbclusters / 4;
	V_ip6_maxfrags = nmbclusters / 4;
	V_ip6q.ip6q_next = V_ip6q.ip6q_prev = &V_ip6q;

	if (!IS_DEFAULT_VNET(curvnet))
		return;

	EVENTHANDLER_REGISTER(nmbclusters_change,
	    frag6_change, NULL, EVENTHANDLER_PRI_ANY);

	IP6Q_LOCK_INIT();
}

/*
 * In RFC2460, fragment and reassembly rule do not agree with each other,
 * in terms of next header field handling in fragment header.
 * While the sender will use the same value for all of the fragmented packets,
 * receiver is suggested not to check the consistency.
 *
 * fragment rule (p20):
 *	(2) A Fragment header containing:
 *	The Next Header value that identifies the first header of
 *	the Fragmentable Part of the original packet.
 *		-> next header field is same for all fragments
 *
 * reassembly rule (p21):
 *	The Next Header field of the last header of the Unfragmentable
 *	Part is obtained from the Next Header field of the first
 *	fragment's Fragment header.
 *		-> should grab it from the first fragment only
 *
 * The following note also contradicts with fragment rule - no one is going to
 * send different fragment with different next header field.
 *
 * additional note (p22):
 *	The Next Header values in the Fragment headers of different
 *	fragments of the same original packet may differ.  Only the value
 *	from the Offset zero fragment packet is used for reassembly.
 *		-> should grab it from the first fragment only
 *
 * There is no explicit reason given in the RFC.  Historical reason maybe?
 */
/*
 * Fragment input
 */
int
frag6_input(struct mbuf **mp, int *offp, int proto)
{
	struct mbuf *m = *mp, *t;
	struct ip6_hdr *ip6;
	struct ip6_frag *ip6f;
	struct ip6q *q6;
	struct ip6asfrag *af6, *ip6af, *af6dwn;
	struct in6_ifaddr *ia;
	int offset = *offp, nxt, i, next;
	int first_frag = 0;
	int fragoff, frgpartlen;	/* must be larger than u_int16_t */
	struct ifnet *dstifp;
	u_int8_t ecn, ecn0;
#ifdef RSS
	struct m_tag *mtag;
	struct ip6_direct_ctx *ip6dc;
#endif

#if 0
	char ip6buf[INET6_ADDRSTRLEN];
#endif

	ip6 = mtod(m, struct ip6_hdr *);
#ifndef PULLDOWN_TEST
	IP6_EXTHDR_CHECK(m, offset, sizeof(struct ip6_frag), IPPROTO_DONE);
	ip6f = (struct ip6_frag *)((caddr_t)ip6 + offset);
#else
	IP6_EXTHDR_GET(ip6f, struct ip6_frag *, m, offset, sizeof(*ip6f));
	if (ip6f == NULL)
		return (IPPROTO_DONE);
#endif

	dstifp = NULL;
	/* find the destination interface of the packet. */
	ia = in6ifa_ifwithaddr(&ip6->ip6_dst, 0 /* XXX */);
	if (ia != NULL) {
		dstifp = ia->ia_ifp;
		ifa_free(&ia->ia_ifa);
	}
	/* jumbo payload can't contain a fragment header */
	if (ip6->ip6_plen == 0) {
		icmp6_error(m, ICMP6_PARAM_PROB, ICMP6_PARAMPROB_HEADER, offset);
		in6_ifstat_inc(dstifp, ifs6_reass_fail);
		return IPPROTO_DONE;
	}

	/*
	 * check whether fragment packet's fragment length is
	 * multiple of 8 octets.
	 * sizeof(struct ip6_frag) == 8
	 * sizeof(struct ip6_hdr) = 40
	 */
	if ((ip6f->ip6f_offlg & IP6F_MORE_FRAG) &&
	    (((ntohs(ip6->ip6_plen) - offset) & 0x7) != 0)) {
		icmp6_error(m, ICMP6_PARAM_PROB, ICMP6_PARAMPROB_HEADER,
		    offsetof(struct ip6_hdr, ip6_plen));
		in6_ifstat_inc(dstifp, ifs6_reass_fail);
		return IPPROTO_DONE;
	}

	IP6STAT_INC(ip6s_fragments);
	in6_ifstat_inc(dstifp, ifs6_reass_reqd);

	/* offset now points to data portion */
	offset += sizeof(struct ip6_frag);

	/*
	 * RFC 6946: Handle "atomic" fragments (offset and m bit set to 0)
	 * upfront, unrelated to any reassembly.  Just skip the fragment header.
	 */
	if ((ip6f->ip6f_offlg & ~IP6F_RESERVED_MASK) == 0) {
		/* XXX-BZ we want dedicated counters for this. */
		IP6STAT_INC(ip6s_reassembled);
		in6_ifstat_inc(dstifp, ifs6_reass_ok);
		*offp = offset;
		return (ip6f->ip6f_nxt);
	}

	IP6Q_LOCK();

	/*
	 * Enforce upper bound on number of fragments.
	 * If maxfrag is 0, never accept fragments.
	 * If maxfrag is -1, accept all fragments without limitation.
	 */
	if (V_ip6_maxfrags < 0)
		;
	else if (V_frag6_nfrags >= (u_int)V_ip6_maxfrags)
		goto dropfrag;

	for (q6 = V_ip6q.ip6q_next; q6 != &V_ip6q; q6 = q6->ip6q_next)
		if (ip6f->ip6f_ident == q6->ip6q_ident &&
		    IN6_ARE_ADDR_EQUAL(&ip6->ip6_src, &q6->ip6q_src) &&
		    IN6_ARE_ADDR_EQUAL(&ip6->ip6_dst, &q6->ip6q_dst)
#ifdef MAC
		    && mac_ip6q_match(m, q6)
#endif
		    )
			break;

	if (q6 == &V_ip6q) {
		/*
		 * the first fragment to arrive, create a reassembly queue.
		 */
		first_frag = 1;

		/*
		 * Enforce upper bound on number of fragmented packets
		 * for which we attempt reassembly;
		 * If maxfragpackets is 0, never accept fragments.
		 * If maxfragpackets is -1, accept all fragments without
		 * limitation.
		 */
		if (V_ip6_maxfragpackets < 0)
			;
		else if (V_frag6_nfragpackets >= (u_int)V_ip6_maxfragpackets)
			goto dropfrag;
		V_frag6_nfragpackets++;
		q6 = (struct ip6q *)malloc(sizeof(struct ip6q), M_FTABLE,
		    M_NOWAIT);
		if (q6 == NULL)
			goto dropfrag;
		bzero(q6, sizeof(*q6));
#ifdef MAC
		if (mac_ip6q_init(q6, M_NOWAIT) != 0) {
			free(q6, M_FTABLE);
			goto dropfrag;
		}
		mac_ip6q_create(m, q6);
#endif
		frag6_insque(q6, &V_ip6q);

		/* ip6q_nxt will be filled afterwards, from 1st fragment */
		q6->ip6q_down	= q6->ip6q_up = (struct ip6asfrag *)q6;
#ifdef notyet
		q6->ip6q_nxtp	= (u_char *)nxtp;
#endif
		q6->ip6q_ident	= ip6f->ip6f_ident;
		q6->ip6q_ttl	= IPV6_FRAGTTL;
		q6->ip6q_src	= ip6->ip6_src;
		q6->ip6q_dst	= ip6->ip6_dst;
		q6->ip6q_ecn	=
		    (ntohl(ip6->ip6_flow) >> 20) & IPTOS_ECN_MASK;
		q6->ip6q_unfrglen = -1;	/* The 1st fragment has not arrived. */

		q6->ip6q_nfrag = 0;
	}

	/*
	 * If it's the 1st fragment, record the length of the
	 * unfragmentable part and the next header of the fragment header.
	 */
	fragoff = ntohs(ip6f->ip6f_offlg & IP6F_OFF_MASK);
	if (fragoff == 0) {
		q6->ip6q_unfrglen = offset - sizeof(struct ip6_hdr) -
		    sizeof(struct ip6_frag);
		q6->ip6q_nxt = ip6f->ip6f_nxt;
	}

	/*
	 * Check that the reassembled packet would not exceed 65535 bytes
	 * in size.
	 * If it would exceed, discard the fragment and return an ICMP error.
	 */
	frgpartlen = sizeof(struct ip6_hdr) + ntohs(ip6->ip6_plen) - offset;
	if (q6->ip6q_unfrglen >= 0) {
		/* The 1st fragment has already arrived. */
		if (q6->ip6q_unfrglen + fragoff + frgpartlen > IPV6_MAXPACKET) {
			icmp6_error(m, ICMP6_PARAM_PROB, ICMP6_PARAMPROB_HEADER,
			    offset - sizeof(struct ip6_frag) +
			    offsetof(struct ip6_frag, ip6f_offlg));
			IP6Q_UNLOCK();
			return (IPPROTO_DONE);
		}
	} else if (fragoff + frgpartlen > IPV6_MAXPACKET) {
		icmp6_error(m, ICMP6_PARAM_PROB, ICMP6_PARAMPROB_HEADER,
		    offset - sizeof(struct ip6_frag) +
		    offsetof(struct ip6_frag, ip6f_offlg));
		IP6Q_UNLOCK();
		return (IPPROTO_DONE);
	}
	/*
	 * If it's the first fragment, do the above check for each
	 * fragment already stored in the reassembly queue.
	 */
	if (fragoff == 0) {
		for (af6 = q6->ip6q_down; af6 != (struct ip6asfrag *)q6;
		     af6 = af6dwn) {
			af6dwn = af6->ip6af_down;

			if (q6->ip6q_unfrglen + af6->ip6af_off + af6->ip6af_frglen >
			    IPV6_MAXPACKET) {
				struct mbuf *merr = IP6_REASS_MBUF(af6);
				struct ip6_hdr *ip6err;
				int erroff = af6->ip6af_offset;

				/* dequeue the fragment. */
				frag6_deq(af6);
				free(af6, M_FTABLE);

				/* adjust pointer. */
				ip6err = mtod(merr, struct ip6_hdr *);

				/*
				 * Restore source and destination addresses
				 * in the erroneous IPv6 header.
				 */
				ip6err->ip6_src = q6->ip6q_src;
				ip6err->ip6_dst = q6->ip6q_dst;

				icmp6_error(merr, ICMP6_PARAM_PROB,
				    ICMP6_PARAMPROB_HEADER,
				    erroff - sizeof(struct ip6_frag) +
				    offsetof(struct ip6_frag, ip6f_offlg));
			}
		}
	}

	ip6af = (struct ip6asfrag *)malloc(sizeof(struct ip6asfrag), M_FTABLE,
	    M_NOWAIT);
	if (ip6af == NULL)
		goto dropfrag;
	bzero(ip6af, sizeof(*ip6af));
	ip6af->ip6af_mff = ip6f->ip6f_offlg & IP6F_MORE_FRAG;
	ip6af->ip6af_off = fragoff;
	ip6af->ip6af_frglen = frgpartlen;
	ip6af->ip6af_offset = offset;
	IP6_REASS_MBUF(ip6af) = m;

	if (first_frag) {
		af6 = (struct ip6asfrag *)q6;
		goto insert;
	}

	/*
	 * Handle ECN by comparing this segment with the first one;
	 * if CE is set, do not lose CE.
	 * drop if CE and not-ECT are mixed for the same packet.
	 */
	ecn = (ntohl(ip6->ip6_flow) >> 20) & IPTOS_ECN_MASK;
	ecn0 = q6->ip6q_ecn;
	if (ecn == IPTOS_ECN_CE) {
		if (ecn0 == IPTOS_ECN_NOTECT) {
			free(ip6af, M_FTABLE);
			goto dropfrag;
		}
		if (ecn0 != IPTOS_ECN_CE)
			q6->ip6q_ecn = IPTOS_ECN_CE;
	}
	if (ecn == IPTOS_ECN_NOTECT && ecn0 != IPTOS_ECN_NOTECT) {
		free(ip6af, M_FTABLE);
		goto dropfrag;
	}

	/*
	 * Find a segment which begins after this one does.
	 */
	for (af6 = q6->ip6q_down; af6 != (struct ip6asfrag *)q6;
	     af6 = af6->ip6af_down)
		if (af6->ip6af_off > ip6af->ip6af_off)
			break;

#if 0
	/*
	 * If there is a preceding segment, it may provide some of
	 * our data already.  If so, drop the data from the incoming
	 * segment.  If it provides all of our data, drop us.
	 */
	if (af6->ip6af_up != (struct ip6asfrag *)q6) {
		i = af6->ip6af_up->ip6af_off + af6->ip6af_up->ip6af_frglen
			- ip6af->ip6af_off;
		if (i > 0) {
			if (i >= ip6af->ip6af_frglen)
				goto dropfrag;
			m_adj(IP6_REASS_MBUF(ip6af), i);
			ip6af->ip6af_off += i;
			ip6af->ip6af_frglen -= i;
		}
	}

	/*
	 * While we overlap succeeding segments trim them or,
	 * if they are completely covered, dequeue them.
	 */
	while (af6 != (struct ip6asfrag *)q6 &&
	       ip6af->ip6af_off + ip6af->ip6af_frglen > af6->ip6af_off) {
		i = (ip6af->ip6af_off + ip6af->ip6af_frglen) - af6->ip6af_off;
		if (i < af6->ip6af_frglen) {
			af6->ip6af_frglen -= i;
			af6->ip6af_off += i;
			m_adj(IP6_REASS_MBUF(af6), i);
			break;
		}
		af6 = af6->ip6af_down;
		m_freem(IP6_REASS_MBUF(af6->ip6af_up));
		frag6_deq(af6->ip6af_up);
	}
#else
	/*
	 * If the incoming framgent overlaps some existing fragments in
	 * the reassembly queue, drop it, since it is dangerous to override
	 * existing fragments from a security point of view.
	 * We don't know which fragment is the bad guy - here we trust
	 * fragment that came in earlier, with no real reason.
	 *
	 * Note: due to changes after disabling this part, mbuf passed to
	 * m_adj() below now does not meet the requirement.
	 */
	if (af6->ip6af_up != (struct ip6asfrag *)q6) {
		i = af6->ip6af_up->ip6af_off + af6->ip6af_up->ip6af_frglen
			- ip6af->ip6af_off;
		if (i > 0) {
#if 0				/* suppress the noisy log */
			log(LOG_ERR, "%d bytes of a fragment from %s "
			    "overlaps the previous fragment\n",
			    i, ip6_sprintf(ip6buf, &q6->ip6q_src));
#endif
			free(ip6af, M_FTABLE);
			goto dropfrag;
		}
	}
	if (af6 != (struct ip6asfrag *)q6) {
		i = (ip6af->ip6af_off + ip6af->ip6af_frglen) - af6->ip6af_off;
		if (i > 0) {
#if 0				/* suppress the noisy log */
			log(LOG_ERR, "%d bytes of a fragment from %s "
			    "overlaps the succeeding fragment",
			    i, ip6_sprintf(ip6buf, &q6->ip6q_src));
#endif
			free(ip6af, M_FTABLE);
			goto dropfrag;
		}
	}
#endif

insert:
#ifdef MAC
	if (!first_frag)
		mac_ip6q_update(m, q6);
#endif

	/*
	 * Stick new segment in its place;
	 * check for complete reassembly.
	 * Move to front of packet queue, as we are
	 * the most recently active fragmented packet.
	 */
	frag6_enq(ip6af, af6->ip6af_up);
	V_frag6_nfrags++;
	q6->ip6q_nfrag++;
#if 0 /* xxx */
	if (q6 != V_ip6q.ip6q_next) {
		frag6_remque(q6);
		frag6_insque(q6, &V_ip6q);
	}
#endif
	next = 0;
	for (af6 = q6->ip6q_down; af6 != (struct ip6asfrag *)q6;
	     af6 = af6->ip6af_down) {
		if (af6->ip6af_off != next) {
			IP6Q_UNLOCK();
			return IPPROTO_DONE;
		}
		next += af6->ip6af_frglen;
	}
	if (af6->ip6af_up->ip6af_mff) {
		IP6Q_UNLOCK();
		return IPPROTO_DONE;
	}

	/*
	 * Reassembly is complete; concatenate fragments.
	 */
	ip6af = q6->ip6q_down;
	t = m = IP6_REASS_MBUF(ip6af);
	af6 = ip6af->ip6af_down;
	frag6_deq(ip6af);
	while (af6 != (struct ip6asfrag *)q6) {
		af6dwn = af6->ip6af_down;
		frag6_deq(af6);
		while (t->m_next)
			t = t->m_next;
		m_adj(IP6_REASS_MBUF(af6), af6->ip6af_offset);
		m_cat(t, IP6_REASS_MBUF(af6));
		free(af6, M_FTABLE);
		af6 = af6dwn;
	}

	/* adjust offset to point where the original next header starts */
	offset = ip6af->ip6af_offset - sizeof(struct ip6_frag);
	free(ip6af, M_FTABLE);
	ip6 = mtod(m, struct ip6_hdr *);
	ip6->ip6_plen = htons((u_short)next + offset - sizeof(struct ip6_hdr));
	if (q6->ip6q_ecn == IPTOS_ECN_CE)
		ip6->ip6_flow |= htonl(IPTOS_ECN_CE << 20);
	nxt = q6->ip6q_nxt;
#ifdef notyet
	*q6->ip6q_nxtp = (u_char)(nxt & 0xff);
#endif

	if (ip6_deletefraghdr(m, offset, M_NOWAIT) != 0) {
		frag6_remque(q6);
		V_frag6_nfrags -= q6->ip6q_nfrag;
#ifdef MAC
		mac_ip6q_destroy(q6);
#endif
		free(q6, M_FTABLE);
		V_frag6_nfragpackets--;

		goto dropfrag;
	}

	/*
	 * Store NXT to the original.
	 */
	{
		char *prvnxtp = ip6_get_prevhdr(m, offset); /* XXX */
		*prvnxtp = nxt;
	}

	frag6_remque(q6);
	V_frag6_nfrags -= q6->ip6q_nfrag;
#ifdef MAC
	mac_ip6q_reassemble(q6, m);
	mac_ip6q_destroy(q6);
#endif
	free(q6, M_FTABLE);
	V_frag6_nfragpackets--;

	if (m->m_flags & M_PKTHDR) { /* Isn't it always true? */
		int plen = 0;
		for (t = m; t; t = t->m_next)
			plen += t->m_len;
		m->m_pkthdr.len = plen;
	}

#ifdef RSS
	mtag = m_tag_alloc(MTAG_ABI_IPV6, IPV6_TAG_DIRECT, sizeof(*ip6dc),
	    M_NOWAIT);
	if (mtag == NULL)
		goto dropfrag;

	ip6dc = (struct ip6_direct_ctx *)(mtag + 1);
	ip6dc->ip6dc_nxt = nxt;
	ip6dc->ip6dc_off = offset;

	m_tag_prepend(m, mtag);
#endif

	IP6Q_UNLOCK();
	IP6STAT_INC(ip6s_reassembled);
	in6_ifstat_inc(dstifp, ifs6_reass_ok);

#ifdef RSS
	/*
	 * Queue/dispatch for reprocessing.
	 */
	netisr_dispatch(NETISR_IPV6_DIRECT, m);
	return IPPROTO_DONE;
#endif

	/*
	 * Tell launch routine the next header
	 */

	*mp = m;
	*offp = offset;

	return nxt;

 dropfrag:
	IP6Q_UNLOCK();
	in6_ifstat_inc(dstifp, ifs6_reass_fail);
	IP6STAT_INC(ip6s_fragdropped);
	m_freem(m);
	return IPPROTO_DONE;
}

/*
 * Free a fragment reassembly header and all
 * associated datagrams.
 */
void
frag6_freef(struct ip6q *q6)
{
	struct ip6asfrag *af6, *down6;

	IP6Q_LOCK_ASSERT();

	for (af6 = q6->ip6q_down; af6 != (struct ip6asfrag *)q6;
	     af6 = down6) {
		struct mbuf *m = IP6_REASS_MBUF(af6);

		down6 = af6->ip6af_down;
		frag6_deq(af6);

		/*
		 * Return ICMP time exceeded error for the 1st fragment.
		 * Just free other fragments.
		 */
		if (af6->ip6af_off == 0) {
			struct ip6_hdr *ip6;

			/* adjust pointer */
			ip6 = mtod(m, struct ip6_hdr *);

			/* restore source and destination addresses */
			ip6->ip6_src = q6->ip6q_src;
			ip6->ip6_dst = q6->ip6q_dst;

			icmp6_error(m, ICMP6_TIME_EXCEEDED,
				    ICMP6_TIME_EXCEED_REASSEMBLY, 0);
		} else
			m_freem(m);
		free(af6, M_FTABLE);
	}
	frag6_remque(q6);
	V_frag6_nfrags -= q6->ip6q_nfrag;
#ifdef MAC
	mac_ip6q_destroy(q6);
#endif
	free(q6, M_FTABLE);
	V_frag6_nfragpackets--;
}

/*
 * Put an ip fragment on a reassembly chain.
 * Like insque, but pointers in middle of structure.
 */
void
frag6_enq(struct ip6asfrag *af6, struct ip6asfrag *up6)
{

	IP6Q_LOCK_ASSERT();

	af6->ip6af_up = up6;
	af6->ip6af_down = up6->ip6af_down;
	up6->ip6af_down->ip6af_up = af6;
	up6->ip6af_down = af6;
}

/*
 * To frag6_enq as remque is to insque.
 */
void
frag6_deq(struct ip6asfrag *af6)
{

	IP6Q_LOCK_ASSERT();

	af6->ip6af_up->ip6af_down = af6->ip6af_down;
	af6->ip6af_down->ip6af_up = af6->ip6af_up;
}

void
frag6_insque(struct ip6q *new, struct ip6q *old)
{

	IP6Q_LOCK_ASSERT();

	new->ip6q_prev = old;
	new->ip6q_next = old->ip6q_next;
	old->ip6q_next->ip6q_prev= new;
	old->ip6q_next = new;
}

void
frag6_remque(struct ip6q *p6)
{

	IP6Q_LOCK_ASSERT();

	p6->ip6q_prev->ip6q_next = p6->ip6q_next;
	p6->ip6q_next->ip6q_prev = p6->ip6q_prev;
}

/*
 * IPv6 reassembling timer processing;
 * if a timer expires on a reassembly
 * queue, discard it.
 */
void
frag6_slowtimo(void)
{
	VNET_ITERATOR_DECL(vnet_iter);
	struct ip6q *q6;

	VNET_LIST_RLOCK_NOSLEEP();
	IP6Q_LOCK();
	VNET_FOREACH(vnet_iter) {
		CURVNET_SET(vnet_iter);
		q6 = V_ip6q.ip6q_next;
		if (q6)
			while (q6 != &V_ip6q) {
				--q6->ip6q_ttl;
				q6 = q6->ip6q_next;
				if (q6->ip6q_prev->ip6q_ttl == 0) {
					IP6STAT_INC(ip6s_fragtimeout);
					/* XXX in6_ifstat_inc(ifp, ifs6_reass_fail) */
					frag6_freef(q6->ip6q_prev);
				}
			}
		/*
		 * If we are over the maximum number of fragments
		 * (due to the limit being lowered), drain off
		 * enough to get down to the new limit.
		 */
		while (V_frag6_nfragpackets > (u_int)V_ip6_maxfragpackets &&
		    V_ip6q.ip6q_prev) {
			IP6STAT_INC(ip6s_fragoverflow);
			/* XXX in6_ifstat_inc(ifp, ifs6_reass_fail) */
			frag6_freef(V_ip6q.ip6q_prev);
		}
		CURVNET_RESTORE();
	}
	IP6Q_UNLOCK();
	VNET_LIST_RUNLOCK_NOSLEEP();
}

/*
 * Drain off all datagram fragments.
 */
void
frag6_drain(void)
{
	VNET_ITERATOR_DECL(vnet_iter);

	VNET_LIST_RLOCK_NOSLEEP();
	if (IP6Q_TRYLOCK() == 0) {
		VNET_LIST_RUNLOCK_NOSLEEP();
		return;
	}
	VNET_FOREACH(vnet_iter) {
		CURVNET_SET(vnet_iter);
		while (V_ip6q.ip6q_next != &V_ip6q) {
			IP6STAT_INC(ip6s_fragdropped);
			/* XXX in6_ifstat_inc(ifp, ifs6_reass_fail) */
			frag6_freef(V_ip6q.ip6q_next);
		}
		CURVNET_RESTORE();
	}
	IP6Q_UNLOCK();
	VNET_LIST_RUNLOCK_NOSLEEP();
}

int
ip6_deletefraghdr(struct mbuf *m, int offset, int wait)
{
	struct ip6_hdr *ip6 = mtod(m, struct ip6_hdr *);
	struct mbuf *t;

	/* Delete frag6 header. */
	if (m->m_len >= offset + sizeof(struct ip6_frag)) {
		/* This is the only possible case with !PULLDOWN_TEST. */
		bcopy(ip6, (char *)ip6 + sizeof(struct ip6_frag),
		    offset);
		m->m_data += sizeof(struct ip6_frag);
		m->m_len -= sizeof(struct ip6_frag);
	} else {
		/* This comes with no copy if the boundary is on cluster. */
		if ((t = m_split(m, offset, wait)) == NULL)
			return (ENOMEM);
		m_adj(t, sizeof(struct ip6_frag));
		m_cat(m, t);
	}

	return (0);
}
