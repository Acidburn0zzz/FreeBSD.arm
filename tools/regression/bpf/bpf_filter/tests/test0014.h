/*-
 * Test 0014:	BPF_STX & BPF_LD+BPF_MEM
 *
 * $FreeBSD: head/tools/regression/bpf/bpf_filter/tests/test0014.h 182393 2008-08-28 18:38:55Z jkim $
 */

/* BPF program */
struct bpf_insn pc[] = {
	BPF_STMT(BPF_LDX+BPF_IMM, 0xdeadc0de),
	BPF_STMT(BPF_STX, 7),
	BPF_STMT(BPF_LD+BPF_MEM, 7),
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
u_int	expect =	0xdeadc0de;

/* Expected signal */
int	expect_signal =	0;
