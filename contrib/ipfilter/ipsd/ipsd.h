/*	$FreeBSD: head/contrib/ipfilter/ipsd/ipsd.h 145519 2005-04-25 18:20:15Z darrenr $	*/

/*
 * (C)opyright 1995-1998 Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * @(#)ipsd.h	1.3 12/3/95
 */

typedef	struct	{
	time_t	sh_date;
	struct	in_addr	sh_ip;
} sdhit_t;

typedef	struct	{
	u_int	sd_sz;
	u_int	sd_cnt;
	u_short	sd_port;
	sdhit_t	*sd_hit;
} ipsd_t;

typedef	struct	{
	struct	in_addr	ss_ip;
	int	ss_hits;
	u_long	ss_ports;
} ipss_t;

