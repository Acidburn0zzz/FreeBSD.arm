/*-
 * Test 0035:	BPF_ALU+BPF_DIV+BPF_K
 *
 * $FreeBSD: head/tools/regression/bpf/bpf_filter/tests/test0035.h 182393 2008-08-28 18:38:55Z jkim $
 */

/* BPF program */
struct bpf_insn pc[] = {
	BPF_STMT(BPF_LD+BPF_IMM, 0xa7c2da06),
	BPF_STMT(BPF_ALU+BPF_DIV+BPF_K, 0xdead),
	BPF_STMT(BPF_RET+BPF_A, 0),
};

/* Packet */
u_char	pkt[] = {
	0x00,
};

/* Packet length seen on wire */
u_int	wirelen =	sizeof(pkt);

/* Packet length passed on buffer */
u_int	buflen =	sizeof(pkt);

/* Invalid instruction */
int	invalid =	0;

/* Expected return value */
u_int	expect =	0xc0de;

/* Expected signal */
int	expect_signal =	0;
