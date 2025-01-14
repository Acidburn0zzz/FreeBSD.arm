/*-
 * Copyright (c) 2002-2008 Sam Leffler, Errno Consulting
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.
 *
 * $FreeBSD: head/tools/tools/ath/common/dumpregs_5212.c 287297 2015-08-29 19:47:20Z rodrigc $
 */

#include <sys/param.h>

#include "diag.h"

#include "ah.h"
#include "ah_internal.h"
#include "ar5212/ar5212reg.h"
#include "ar5212/ar5212phy.h"

#include "dumpregs.h"

#define	MAC5212	SREV(4,5), SREV(16,0)
#define	MAC5213	SREV(5,9), SREV(16,0)

static struct dumpreg ar5212regs[] = {
    DEFBASIC(AR_CR,		"CR"),
    DEFBASIC(AR_RXDP,		"RXDP"),
    DEFBASICfmt(AR_CFG,		"CFG",
	   "\20\1SWTD\2SWTB\3SWRD\4SWRB\5SWRG\6AP_ADHOC\11PHOK\12EEBS"),
    DEFBASIC(AR_IER,		"IER"),
    DEFBASIC(AR_TXCFG,		"TXCFG"),
    DEFBASICfmt(AR_RXCFG,	"RXCFG",
	  "\20\6JUMBO_ENA\7JUMBO_WRAP\10SLEEP_DEBUG"),
    DEFBASIC(AR_MIBC,		"MIBC"),
    DEFBASIC(AR_TOPS,		"TOPS"),
    DEFBASIC(AR_RXNPTO,		"RXNPTO"),
    DEFBASIC(AR_TXNPTO,		"TXNPTO"),
    DEFBASIC(AR_RPGTO,		"RPGTO"),
    DEFBASIC(AR_RPCNT,		"RPCNT"),
    DEFBASIC(AR_MACMISC,	"MACMISC"),
    DEFBASIC(AR_SPC_0,		"SPC_0"),
    DEFBASIC(AR_SPC_1,		"SPC_1"),

    DEFINTfmt(AR_ISR,		"ISR",
	  "\20\1RXOK\2RXDESC\3RXERR\4RXNOPKT\5RXEOL\6RXORN\7TXOK\10TXDESC"
	  "\11TXERR\12TXNOPKT\13TXEOL\14TXURN\15MIB\16SWI\17RXPHY\20RXKCM"
	  "\21SWBA\22BRSSI\23BMISS\24HIUERR\25BNR\26RXCHIRP\27RXDOPPL\30BCNMISS"
	  "\31TIM\32GPIO\33QCBROVF\34QCBRURN\35QTRIG"),
    DEFINT(AR_ISR_S0,		"ISR_S0"),
    DEFINT(AR_ISR_S1,		"ISR_S1"),
    DEFINTfmt(AR_ISR_S2,	"ISR_S2",
	  "\20\21MCABT\22SSERR\23DPERR\24TIM\25CABEND\26DTIMSYNC\27BCNTO"
	  "\30CABTO\31DTIM"),
    DEFINT(AR_ISR_S3,		"ISR_S3"),
    DEFINT(AR_ISR_S4,		"ISR_S4"),
    DEFINTfmt(AR_IMR,		"IMR",
	  "\20\1RXOK\2RXDESC\3RXERR\4RXNOPKT\5RXEOL\6RXORN\7TXOK\10TXDESC"
	  "\11TXERR\12TXNOPKT\13TXEOL\14TXURN\15MIB\16SWI\17RXPHY\20RXKCM"
	  "\21SWBA\22BRSSI\23BMISS\24HIUERR\25BNR\26RXCHIRP\27RXDOPPL\30BCNMISS"
	  "\31TIM\32GPIO\33QCBROVF\34QCBRURN\35QTRIG"),
    DEFINT(AR_IMR_S0,		"IMR_S0"),
    DEFINT(AR_IMR_S1,		"IMR_S1"),
    DEFINTfmt(AR_IMR_S2,	"IMR_S2",
	  "\20\21MCABT\22SSERR\23DPERR\24TIM\25CABEND\26DTIMSYNC\27BCNTO"
	  "\30CABTO\31DTIM"),
    DEFINT(AR_IMR_S3,		"IMR_S3"),
    DEFINT(AR_IMR_S4,		"IMR_S4"),
    /* NB: don't read the RAC so we don't affect operation */
    DEFVOID(AR_ISR_RAC,		"ISR_RAC"),
    DEFINT(AR_ISR_S0_S,		"ISR_S0_S"),
    DEFINT(AR_ISR_S1_S,		"ISR_S1_S"),
    DEFINT(AR_ISR_S2_S,		"ISR_S2_S"),
    DEFINT(AR_ISR_S3_S,		"ISR_S3_S"),
    DEFINT(AR_ISR_S4_S,		"ISR_S4_S"),

    DEFBASIC(AR_DMADBG_0,	"DMADBG0"),
    DEFBASIC(AR_DMADBG_1,	"DMADBG1"),
    DEFBASIC(AR_DMADBG_2,	"DMADBG2"),
    DEFBASIC(AR_DMADBG_3,	"DMADBG3"),
    DEFBASIC(AR_DMADBG_4,	"DMADBG4"),
    DEFBASIC(AR_DMADBG_5,	"DMADBG5"),
    DEFBASIC(AR_DMADBG_6,	"DMADBG6"),
    DEFBASIC(AR_DMADBG_7,	"DMADBG7"),

    DEFBASIC(AR_DCM_A,		"DCM_A"),
    DEFBASIC(AR_DCM_D,		"DCM_D"),
    DEFBASIC(AR_DCCFG,		"DCCFG"),
    DEFBASIC(AR_CCFG,		"CCFG"),
    DEFBASIC(AR_CCUCFG,		"CCUCFG"),
    DEFBASIC(AR_CPC_0,		"CPC0"),
    DEFBASIC(AR_CPC_1,		"CPC1"),
    DEFBASIC(AR_CPC_2,		"CPC2"),
    DEFBASIC(AR_CPC_3,		"CPC3"),
    DEFBASIC(AR_CPCOVF,		"CPCOVF"),

    DEFQCU(AR_Q0_TXDP,		"Q0_TXDP"),
    DEFQCU(AR_Q1_TXDP,		"Q1_TXDP"),
    DEFQCU(AR_Q2_TXDP,		"Q2_TXDP"),
    DEFQCU(AR_Q3_TXDP,		"Q3_TXDP"),
    DEFQCU(AR_Q4_TXDP,		"Q4_TXDP"),
    DEFQCU(AR_Q5_TXDP,		"Q5_TXDP"),
    DEFQCU(AR_Q6_TXDP,		"Q6_TXDP"),
    DEFQCU(AR_Q7_TXDP,		"Q7_TXDP"),
    DEFQCU(AR_Q8_TXDP,		"Q8_TXDP"),
    DEFQCU(AR_Q9_TXDP,		"Q9_TXDP"),

    DEFQCU(AR_Q_TXE,		"Q_TXE"),
    DEFQCU(AR_Q_TXD,		"Q_TXD"),

    DEFQCU(AR_Q0_CBRCFG,	"Q0_CBR"),
    DEFQCU(AR_Q1_CBRCFG,	"Q1_CBR"),
    DEFQCU(AR_Q2_CBRCFG,	"Q2_CBR"),
    DEFQCU(AR_Q3_CBRCFG,	"Q3_CBR"),
    DEFQCU(AR_Q4_CBRCFG,	"Q4_CBR"),
    DEFQCU(AR_Q5_CBRCFG,	"Q5_CBR"),
    DEFQCU(AR_Q6_CBRCFG,	"Q6_CBR"),
    DEFQCU(AR_Q7_CBRCFG,	"Q7_CBR"),
    DEFQCU(AR_Q8_CBRCFG,	"Q8_CBR"),
    DEFQCU(AR_Q9_CBRCFG,	"Q9_CBR"),

    DEFQCU(AR_Q0_RDYTIMECFG,	"Q0_RDYT"),
    DEFQCU(AR_Q1_RDYTIMECFG,	"Q1_RDYT"),
    DEFQCU(AR_Q2_RDYTIMECFG,	"Q2_RDYT"),
    DEFQCU(AR_Q3_RDYTIMECFG,	"Q3_RDYT"),
    DEFQCU(AR_Q4_RDYTIMECFG,	"Q4_RDYT"),
    DEFQCU(AR_Q5_RDYTIMECFG,	"Q5_RDYT"),
    DEFQCU(AR_Q6_RDYTIMECFG,	"Q6_RDYT"),
    DEFQCU(AR_Q7_RDYTIMECFG,	"Q7_RDYT"),
    DEFQCU(AR_Q8_RDYTIMECFG,	"Q8_RDYT"),
    DEFQCU(AR_Q9_RDYTIMECFG,	"Q9_RDYT"),

    DEFQCU(AR_Q_ONESHOTARM_SC,	"Q_ONESHOTARM_SC"),
    DEFQCU(AR_Q_ONESHOTARM_CC,	"Q_ONESHOTARM_CC"),

    DEFQCU(AR_Q0_MISC,		"Q0_MISC"),
    DEFQCU(AR_Q1_MISC,		"Q1_MISC"),
    DEFQCU(AR_Q2_MISC,		"Q2_MISC"),
    DEFQCU(AR_Q3_MISC,		"Q3_MISC"),
    DEFQCU(AR_Q4_MISC,		"Q4_MISC"),
    DEFQCU(AR_Q5_MISC,		"Q5_MISC"),
    DEFQCU(AR_Q6_MISC,		"Q6_MISC"),
    DEFQCU(AR_Q7_MISC,		"Q7_MISC"),
    DEFQCU(AR_Q8_MISC,		"Q8_MISC"),
    DEFQCU(AR_Q9_MISC,		"Q9_MISC"),

    DEFQCU(AR_Q0_STS,		"Q0_STS"),
    DEFQCU(AR_Q1_STS,		"Q1_STS"),
    DEFQCU(AR_Q2_STS,		"Q2_STS"),
    DEFQCU(AR_Q3_STS,		"Q3_STS"),
    DEFQCU(AR_Q4_STS,		"Q4_STS"),
    DEFQCU(AR_Q5_STS,		"Q5_STS"),
    DEFQCU(AR_Q6_STS,		"Q6_STS"),
    DEFQCU(AR_Q7_STS,		"Q7_STS"),
    DEFQCU(AR_Q8_STS,		"Q8_STS"),
    DEFQCU(AR_Q9_STS,		"Q9_STS"),

    DEFQCU(AR_Q_RDYTIMESHDN,	"Q_RDYTIMSHD"),

    DEFQCU(AR_Q_CBBS,		"Q_CBBS"),
    DEFQCU(AR_Q_CBBA,		"Q_CBBA"),
    DEFQCU(AR_Q_CBC,		"Q_CBC"),

    DEFDCU(AR_D0_QCUMASK,	"D0_MASK"),
    DEFDCU(AR_D1_QCUMASK,	"D1_MASK"),
    DEFDCU(AR_D2_QCUMASK,	"D2_MASK"),
    DEFDCU(AR_D3_QCUMASK,	"D3_MASK"),
    DEFDCU(AR_D4_QCUMASK,	"D4_MASK"),
    DEFDCU(AR_D5_QCUMASK,	"D5_MASK"),
    DEFDCU(AR_D6_QCUMASK,	"D6_MASK"),
    DEFDCU(AR_D7_QCUMASK,	"D7_MASK"),
    DEFDCU(AR_D8_QCUMASK,	"D8_MASK"),
    DEFDCU(AR_D9_QCUMASK,	"D9_MASK"),

    DEFDCU(AR_D0_LCL_IFS,	"D0_IFS"),
    DEFDCU(AR_D1_LCL_IFS,	"D1_IFS"),
    DEFDCU(AR_D2_LCL_IFS,	"D2_IFS"),
    DEFDCU(AR_D3_LCL_IFS,	"D3_IFS"),
    DEFDCU(AR_D4_LCL_IFS,	"D4_IFS"),
    DEFDCU(AR_D5_LCL_IFS,	"D5_IFS"),
    DEFDCU(AR_D6_LCL_IFS,	"D6_IFS"),
    DEFDCU(AR_D7_LCL_IFS,	"D7_IFS"),
    DEFDCU(AR_D8_LCL_IFS,	"D8_IFS"),
    DEFDCU(AR_D9_LCL_IFS,	"D9_IFS"),

    DEFDCU(AR_D0_RETRY_LIMIT,	"D0_RTRY"),
    DEFDCU(AR_D1_RETRY_LIMIT,	"D1_RTRY"),
    DEFDCU(AR_D2_RETRY_LIMIT,	"D2_RTRY"),
    DEFDCU(AR_D3_RETRY_LIMIT,	"D3_RTRY"),
    DEFDCU(AR_D4_RETRY_LIMIT,	"D4_RTRY"),
    DEFDCU(AR_D5_RETRY_LIMIT,	"D5_RTRY"),
    DEFDCU(AR_D6_RETRY_LIMIT,	"D6_RTRY"),
    DEFDCU(AR_D7_RETRY_LIMIT,	"D7_RTRY"),
    DEFDCU(AR_D8_RETRY_LIMIT,	"D8_RTRY"),
    DEFDCU(AR_D9_RETRY_LIMIT,	"D9_RTRY"),

    DEFDCU(AR_D0_CHNTIME,	"D0_CHNT"),
    DEFDCU(AR_D1_CHNTIME,	"D1_CHNT"),
    DEFDCU(AR_D2_CHNTIME,	"D2_CHNT"),
    DEFDCU(AR_D3_CHNTIME,	"D3_CHNT"),
    DEFDCU(AR_D4_CHNTIME,	"D4_CHNT"),
    DEFDCU(AR_D5_CHNTIME,	"D5_CHNT"),
    DEFDCU(AR_D6_CHNTIME,	"D6_CHNT"),
    DEFDCU(AR_D7_CHNTIME,	"D7_CHNT"),
    DEFDCU(AR_D8_CHNTIME,	"D8_CHNT"),
    DEFDCU(AR_D9_CHNTIME,	"D9_CHNT"),

    DEFDCU(AR_D0_MISC,		"D0_MISC"),
    DEFDCU(AR_D1_MISC,		"D1_MISC"),
    DEFDCU(AR_D2_MISC,		"D2_MISC"),
    DEFDCU(AR_D3_MISC,		"D3_MISC"),
    DEFDCU(AR_D4_MISC,		"D4_MISC"),
    DEFDCU(AR_D5_MISC,		"D5_MISC"),
    DEFDCU(AR_D6_MISC,		"D6_MISC"),
    DEFDCU(AR_D7_MISC,		"D7_MISC"),
    DEFDCU(AR_D8_MISC,		"D8_MISC"),
    DEFDCU(AR_D9_MISC,		"D9_MISC"),

    _DEFREG(AR_D_SEQNUM,	"D_SEQ",	DUMP_BASIC | DUMP_DCU),
    DEFBASIC(AR_D_GBL_IFS_SIFS,	"D_SIFS"),
    DEFBASIC(AR_D_GBL_IFS_SLOT,	"D_SLOT"),
    DEFBASIC(AR_D_GBL_IFS_EIFS,	"D_EIFS"),
    DEFBASIC(AR_D_GBL_IFS_MISC,	"D_MISC"),
    DEFBASIC(AR_D_FPCTL,	"D_FPCTL"),
    DEFBASIC(AR_D_TXPSE,	"D_TXPSE"),
    DEFVOID(AR_D_TXBLK_CMD,	"D_CMD"),
#if 0
    DEFVOID(AR_D_TXBLK_DATA,	"D_DATA"),
#endif
    DEFVOID(AR_D_TXBLK_CLR,	"D_CLR"),
    DEFVOID(AR_D_TXBLK_SET,	"D_SET"),
    DEFBASIC(AR_RC,		"RC"),
    DEFBASICfmt(AR_SCR,		"SCR",
	  "\20\22SLDTP\23SLDWP\24SLEPOL\25MIBIE"),
    DEFBASIC(AR_INTPEND,	"INTPEND"),
    DEFBASIC(AR_SFR,		"SFR"),
    DEFBASIC(AR_PCICFG,		"PCICFG"),
    DEFBASIC(AR_GPIOCR,		"GPIOCR"),
    DEFBASIC(AR_GPIODO,		"GPIODO"),
    DEFBASIC(AR_GPIODI,		"GPIODI"),
    DEFBASIC(AR_SREV,		"SREV"),

    DEFBASICx(AR_PCIE_PMC,	"PCIEPMC", SREV(4,8), SREV(13,7)),
    DEFBASICx(AR_PCIE_SERDES,	"SERDES",  SREV(4,8), SREV(13,7)),
    DEFBASICx(AR_PCIE_SERDES2,	"SERDES2", SREV(4,8), SREV(13,7)),
    DEFVOID(AR_EEPROM_ADDR,	"EEADDR"),
    DEFVOID(AR_EEPROM_DATA,	"EEDATA"),
    DEFVOID(AR_EEPROM_CMD,	"EECMD"),
    DEFVOID(AR_EEPROM_STS,	"EESTS"),
    DEFVOID(AR_EEPROM_CFG,	"EECFG"),
    DEFBASIC(AR_STA_ID0,	"STA_ID0"),
    DEFBASICfmt(AR_STA_ID1,	"STA_ID1",
	  "\20\21STA_AP\22ADHOC\23PWR_SAV\24KSRCHDIS\25PCF\26USE_DEFANT"
	  "\27UPD_DEFANT\30RTS_USE_DEF\31ACKCTS_6MB\32BASE_RATE11B\33USE_DA_SG"
	  "\34CRPT_MIC_ENABLE\35KSRCH_MODE\36PRE_SEQNUM\37CBCIV_ENDIAN"
	  "\40MCAST_KSRCH"),
    DEFBASIC(AR_BSS_ID0,	"BSS_ID0"),
    DEFBASIC(AR_BSS_ID1,	"BSS_ID1"),
    DEFBASIC(AR_SLOT_TIME,	"SLOTTIME"),
    DEFBASIC(AR_TIME_OUT,	"TIME_OUT"),
    DEFBASIC(AR_RSSI_THR,	"RSSI_THR"),
    DEFBASIC(AR_USEC,		"USEC"),
    DEFBASIC(AR_BEACON,		"BEACON"),
    DEFBASIC(AR_CFP_PERIOD,	"CFP_PER"),
    DEFBASIC(AR_TIMER0,		"TIMER0"),
    DEFBASIC(AR_TIMER1,		"TIMER1"),
    DEFBASIC(AR_TIMER2,		"TIMER2"),
    DEFBASIC(AR_TIMER3,		"TIMER3"),
    DEFBASIC(AR_CFP_DUR,	"CFP_DUR"),
    DEFBASICfmt(AR_RX_FILTER,	"RXFILTER",
	  "\20\1UCAST\2MCAST\3BCAST\4CONTROL\5BEACON\6PROM\7XR_POLL\10PROBE_REQ"),
    DEFBASIC(AR_MCAST_FIL0,	"MCAST_0"),
    DEFBASIC(AR_MCAST_FIL1,	"MCAST_1"),
    DEFBASICfmt(AR_DIAG_SW,	"DIAG_SW",
	  "\20\1CACHE_ACK\2ACK_DIS\3CTS_DIS\4ENCRYPT_DIS\5DECRYPT_DIS\6RX_DIS"
	  "\7CORR_FCS\10CHAN_INFO\11EN_SCRAMSD\22FRAME_NV0\25RX_CLR_HI"
	  "\26IGNORE_CS\27CHAN_IDLE\30PHEAR_ME"),
    DEFBASIC(AR_TSF_L32,	"TSF_L32"),
    DEFBASIC(AR_TSF_U32,	"TSF_U32"),
    DEFBASIC(AR_TST_ADDAC,	"TST_ADAC"),
    DEFBASIC(AR_DEF_ANTENNA,	"DEF_ANT"),
    DEFBASIC(AR_QOS_MASK,	"QOS_MASK"),
    DEFBASIC(AR_SEQ_MASK,	"SEQ_MASK"),
    DEFBASIC(AR_OBSERV_2,	"OBSERV2"),
    DEFBASIC(AR_OBSERV_1,	"OBSERV1"),

    DEFBASIC(AR_LAST_TSTP,	"LAST_TST"),
    DEFBASIC(AR_NAV,		"NAV"),
    DEFBASIC(AR_RTS_OK,		"RTS_OK"),
    DEFBASIC(AR_RTS_FAIL,	"RTS_FAIL"),
    DEFBASIC(AR_ACK_FAIL,	"ACK_FAIL"),
    DEFBASIC(AR_FCS_FAIL,	"FCS_FAIL"),
    DEFBASIC(AR_BEACON_CNT,	"BEAC_CNT"),

    DEFBASIC(AR_SLEEP1,		"SLEEP1"),
    DEFBASIC(AR_SLEEP2,		"SLEEP2"),
    DEFBASIC(AR_SLEEP3,		"SLEEP3"),
    DEFBASIC(AR_BSSMSKL,	"BSSMSKL"),
    DEFBASIC(AR_BSSMSKU,	"BSSMSKU"),
    DEFBASIC(AR_TPC,		"TPC"),
    DEFBASIC(AR_TFCNT,		"TFCNT"),
    DEFBASIC(AR_RFCNT,		"RFCNT"),
    DEFBASIC(AR_RCCNT,		"RCCNT"),
    DEFBASIC(AR_CCCNT,		"CCCNT"),
    DEFBASIC(AR_QUIET1,		"QUIET1"),
    DEFBASIC(AR_QUIET2,		"QUIET2"),
    DEFBASIC(AR_TSF_PARM,	"TSF_PARM"),
    DEFBASIC(AR_NOACK,		"NOACK"),
    DEFBASIC(AR_PHY_ERR,	"PHY_ERR"),
    DEFBASIC(AR_QOS_CONTROL,	"QOS_CTRL"),
    DEFBASIC(AR_QOS_SELECT,	"QOS_SEL"),
    DEFBASIC(AR_MISC_MODE,	"MISCMODE"),
    DEFBASIC(AR_FILTOFDM,	"FILTOFDM"),
    DEFBASIC(AR_FILTCCK,	"FILTCCK"),
    DEFBASIC(AR_PHYCNT1,	"PHYCNT1"),
    DEFBASIC(AR_PHYCNTMASK1,	"PHYCMSK1"),
    DEFBASIC(AR_PHYCNT2,	"PHYCNT2"),
    DEFBASIC(AR_PHYCNTMASK2,	"PHYCMSK2"),

    DEFVOID(AR_PHYCNT1,		"PHYCNT1"),
    DEFVOID(AR_PHYCNTMASK1,	"PHYCNTMASK1"),
    DEFVOID(AR_PHYCNT2,		"PHYCNT2"),
    DEFVOID(AR_PHYCNTMASK2,	"PHYCNTMASK2"),

    DEFVOID(AR_PHY_TEST,	"PHY_TEST"),
    DEFVOID(AR_PHY_TURBO,	"PHY_TURBO"),
    DEFVOID(AR_PHY_TESTCTRL,	"PHY_TESTCTRL"),
    DEFVOID(AR_PHY_TIMING3,	"PHY_TIMING3"),
    DEFVOID(AR_PHY_CHIP_ID,	"PHY_CHIP_ID"),
    DEFVOIDfmt(AR_PHY_ACTIVE,	"PHY_ACTIVE",	"\20\1ENA"),
    DEFVOID(AR_PHY_TX_CTL,	"PHY_TX_CTL"),
    DEFVOID(AR_PHY_ADC_CTL,	"PHY_ADC_CTL"),
    DEFVOID(AR_PHY_BB_XP_PA_CTL,"PHY_BB_XP_PA_CTL"),
    DEFVOID(AR_PHY_TSTDAC_CONST,"PHY_TSTDAC_CONST"),
    DEFVOID(AR_PHY_SETTLING,	"PHY_SETTLING"),
    DEFVOID(AR_PHY_RXGAIN,	"PHY_RXGAIN"),
    DEFVOID(AR_PHY_DESIRED_SZ,	"PHY_DESIRED_SZ"),
    DEFVOID(AR_PHY_FIND_SIG,	"PHY_FIND_SIG"),
    DEFVOID(AR_PHY_AGC_CTL1,	"PHY_AGC_CTL1"),
    DEFVOIDfmt(AR_PHY_AGC_CONTROL,	"PHY_AGC_CONTROL",
      "\20\1CAL\2NF\16ENA_NF\22NO_UPDATE_NF"),
    DEFVOIDfmt(AR_PHY_SFCORR_LOW,	"PHY_SFCORR_LOW",
      "\20\1USE_SELF_CORR_LOW"),
    DEFVOID(AR_PHY_SFCORR,	"PHY_SFCORR"),
    DEFVOID(AR_PHY_SLEEP_CTR_CONTROL, "PHY_SLEEP_CTR_CONTROL"),
    DEFVOID(AR_PHY_SLEEP_CTR_LIMIT, "PHY_SLEEP_CTR_LIMIT"),
    DEFVOID(AR_PHY_SLEEP_SCAL,	"PHY_SLEEP_SCAL"),
    DEFVOID(AR_PHY_BIN_MASK_1,	"PHY_BIN_MASK_1"),
    DEFVOID(AR_PHY_BIN_MASK_2,	"PHY_BIN_MASK_2"),
    DEFVOID(AR_PHY_BIN_MASK_3,	"PHY_BIN_MASK_3"),
    DEFVOID(AR_PHY_MASK_CTL,	"PHY_MASK_CTL"),
    DEFVOID(AR_PHY_PLL_CTL,	"PHY_PLL_CTL"),
    DEFVOID(AR_PHY_RX_DELAY,	"PHY_RX_DELAY"),
    DEFVOID(AR_PHY_TIMING_CTRL4,"PHY_TIMING_CTRL4"),
    DEFVOID(AR_PHY_TIMING5,	"PHY_TIMING5"),
    DEFVOID(AR_PHY_PAPD_PROBE,	"PHY_PAPD_PROBE"),
    DEFVOID(AR_PHY_POWER_TX_RATE1,"PHY_POWER_TX_RATE1"),
    DEFVOID(AR_PHY_POWER_TX_RATE2,"PHY_POWER_TX_RATE2"),
    DEFVOID(AR_PHY_POWER_TX_RATE_MAX, "PHY_POWER_TX_RATE_MAX"),
    DEFVOID(AR_PHY_FRAME_CTL,	"PHY_FRAME_CTL"),
    DEFVOID(AR_PHY_TXPWRADJ,	"PHY_TXPWRADJ"),
    DEFVOID(AR_PHY_RADAR_0,	"PHY_RADAR_0"),
    DEFVOID(AR_PHY_SIGMA_DELTA,	"PHY_SIGMA_DELTA"),
    DEFVOID(AR_PHY_RESTART,	"PHY_RESTART"),
    DEFVOID(AR_PHY_RFBUS_REQ,	"PHY_RFBUS_REQ"),
    DEFVOID(AR_PHY_TIMING7,	"PHY_TIMING7"),
    DEFVOID(AR_PHY_TIMING8,	"PHY_TIMING8"),
    DEFVOID(AR_PHY_BIN_MASK2_1,	"PHY_BIN_MASK2_1"),
    DEFVOID(AR_PHY_BIN_MASK2_2,	"PHY_BIN_MASK2_2"),
    DEFVOID(AR_PHY_BIN_MASK2_3,	"PHY_BIN_MASK2_3"),
    DEFVOID(AR_PHY_BIN_MASK2_4,	"PHY_BIN_MASK2_4"),
    DEFVOID(AR_PHY_TIMING9,	"PHY_TIMING9"),
    DEFVOID(AR_PHY_TIMING10,	"PHY_TIMING10"),
    DEFVOID(AR_PHY_TIMING11,	"PHY_TIMING11"),
    DEFVOID(AR_PHY_HEAVY_CLIP_ENABLE, "PHY_HEAVY_CLIP_ENABLE"),
    DEFVOID(AR_PHY_M_SLEEP,	"PHY_M_SLEEP"),
    DEFVOID(AR_PHY_REFCLKDLY,	"PHY_REFCLKDLY"),
    DEFVOID(AR_PHY_REFCLKPD,	"PHY_REFCLKPD"),
    DEFVOID(AR_PHY_IQCAL_RES_PWR_MEAS_I, "PHY_IQCAL_RES_PWR_MEAS_I"),
    DEFVOID(AR_PHY_IQCAL_RES_PWR_MEAS_Q, "PHY_IQCAL_RES_PWR_MEAS_Q"),
    DEFVOID(AR_PHY_IQCAL_RES_IQ_CORR_MEAS, "PHY_IQCAL_RES_IQ_CORR_MEAS"),
    DEFVOID(AR_PHY_CURRENT_RSSI,"PHY_CURRENT_RSSI"),
    DEFVOID(AR_PHY_RFBUS_GNT,	"PHY_RFBUS_GNT"),
    DEFVOIDfmt(AR_PHY_MODE,	"PHY_MODE",
	"\20\1CCK\2RF2GHZ\3DYNAMIC\4AR5112\5HALF\6QUARTER"),
    DEFVOID(AR_PHY_CCK_TX_CTRL,	"PHY_CCK_TX_CTRL"),
    DEFVOID(AR_PHY_CCK_DETECT,	"PHY_CCK_DETECT"),
    DEFVOID(AR_PHY_GAIN_2GHZ,	"PHY_GAIN_2GHZ"),
    DEFVOID(AR_PHY_CCK_RXCTRL4,	"PHY_CCK_RXCTRL4"),
    DEFVOID(AR_PHY_DAG_CTRLCCK,	"PHY_DAG_CTRLCCK"),
    DEFVOID(AR_PHY_DAG_CTRLCCK,	"PHY_DAG_CTRLCCK"),
    DEFVOID(AR_PHY_POWER_TX_RATE3,"PHY_POWER_TX_RATE3"),
    DEFVOID(AR_PHY_POWER_TX_RATE4,"PHY_POWER_TX_RATE4"),
    DEFVOID(AR_PHY_FAST_ADC,	"PHY_FAST_ADC"),
    DEFVOID(AR_PHY_BLUETOOTH,	"PHY_BLUETOOTH"),
    DEFVOID(AR_PHY_TPCRG1,	"PHY_TPCRG1"),
    DEFVOID(AR_PHY_TPCRG5,	"PHY_TPCRG5"),

    /* XXX { AR_RATE_DURATION(0), AR_RATE_DURATION(0x20) }, */
};

static __constructor void
ar5212_ctor(void)
{
	register_regs(ar5212regs, nitems(ar5212regs), MAC5212, PHYANY);
	register_keycache(128, MAC5212, PHYANY);

	register_range(0x9800, 0x987c, DUMP_BASEBAND, MAC5212, PHYANY);
	register_range(0x9900, 0x995c, DUMP_BASEBAND, MAC5212, PHYANY);
	register_range(0x9c00, 0x9c1c, DUMP_BASEBAND, MAC5212, PHYANY);
	register_range(0xa180, 0xa238, DUMP_BASEBAND, MAC5212, PHYANY);
	register_range(0xa258, 0xa26c, DUMP_BASEBAND,
	    SREV(7,8), SREV(15,15), PHYANY);
}
