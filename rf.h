#ifndef ___RF_H
#define ___RF_H

#include "task.h"
#include "interface.h"

__sfr __at 0xE9 RFIRQF0;
__sfr __at 0x91 RFIRQF1;
__sfr __at 0xbf RFERRF;
__sfr __at 0xd9 RFD;
__sfr __at 0xe1 RFST;
__sfr __at 0xbc RNDL;
__sfr __at 0xbd RNDH;
__sfr __at 0xb1 ENCDI;
__sfr __at 0xb2 ENCDO;
__sfr __at 0xb3 ENCCS;
__sfr __at 0x9a IEN2;
__sfr __at 0x9b S1CON;


extern __pdata u8 rf_id[4];

volatile __xdata __at  0x6180 unsigned char  FRMFILT0;
volatile __xdata __at  0x6181 unsigned char  FRMFILT1;
volatile __xdata __at  0x6182 unsigned char  SRCMATCH;
volatile __xdata __at  0x6183 unsigned char  SRCSHORTEN0;
volatile __xdata __at  0x6184 unsigned char  SRCSHORTEN1;
volatile __xdata __at  0x6185 unsigned char  SRCSHORTEN2;
volatile __xdata __at  0x6186 unsigned char  SRCEXTEN0;
volatile __xdata __at  0x6187 unsigned char  SRCEXTEN1;
volatile __xdata __at  0x6188 unsigned char  SRCEXTEN2;
volatile __xdata __at  0x6189 unsigned char  FRMCTRL0;
volatile __xdata __at  0x618a unsigned char  FRMCTRL1;
volatile __xdata __at  0x618b unsigned char  RXENABLE;
volatile __xdata __at  0x618c unsigned char  RXMASKSET;
volatile __xdata __at  0x618d unsigned char  RXMASKCLR;
volatile __xdata __at  0x618e unsigned char  FREQTUNE;
volatile __xdata __at  0x618f unsigned char  FREQCTRL;
volatile __xdata __at  0x6190 unsigned char  TXPOWER;
volatile __xdata __at  0x6191 unsigned char  TXCTRL;
volatile __xdata __at  0x6192 unsigned char  FSMSTAT0;
volatile __xdata __at  0x6193 unsigned char  FSMSTAT1;
volatile __xdata __at  0x6194 unsigned char  FIFOPCTRL;
volatile __xdata __at  0x6195 unsigned char  FSMCTRL;
volatile __xdata __at  0x6196 unsigned char  CCACTRL0;
volatile __xdata __at  0x6197 unsigned char  CCACTRL1;
volatile __xdata __at  0x6198 unsigned char  RSSI;
volatile __xdata __at  0x6199 unsigned char  RSSISTAT;
volatile __xdata __at  0x619a unsigned char  RXFIRST;
volatile __xdata __at  0x619b unsigned char  RXFIFOCNT;
volatile __xdata __at  0x619c unsigned char  TXFIFOCNT;
volatile __xdata __at  0x619d unsigned char  RXFIRST_PTR;
volatile __xdata __at  0x619e unsigned char  RXLAST_PTR;
volatile __xdata __at  0x619f unsigned char  RXP1_PTR;
volatile __xdata __at  0x61a1 unsigned char  TXFIRST_PTR;
volatile __xdata __at  0x61a2 unsigned char  TXLAST_PTR;
volatile __xdata __at  0x61a3 unsigned char  RFIRQM0;
volatile __xdata __at  0x61a4 unsigned char  RFIRQM1;
volatile __xdata __at  0x61a5 unsigned char  RFERRM;
volatile __xdata __at  0x61a6 unsigned char  MONMUX;
volatile __xdata __at  0x61a7 unsigned char  RFRND;
volatile __xdata __at  0x61a8 unsigned char  MDMCTRL0;
volatile __xdata __at  0x61a9 unsigned char  MDMCTRL1;
volatile __xdata __at  0x61aa unsigned char  FREQEST;
volatile __xdata __at  0x61ab unsigned char  RXCTRL;
volatile __xdata __at  0x61ac unsigned char  FSCTRL;
volatile __xdata __at  0x61ae unsigned char  FSCAL1;
volatile __xdata __at  0x61af unsigned char  FSCAL2;
volatile __xdata __at  0x61b0 unsigned char  FSCAL3;
volatile __xdata __at  0x61b1 unsigned char  AGCCTRL0;
volatile __xdata __at  0x61b2 unsigned char  AGCCTRL1;
volatile __xdata __at  0x61b3 unsigned char  AGCCTRL2;
volatile __xdata __at  0x61b4 unsigned char  AGCCTRL3;
volatile __xdata __at  0x61b5 unsigned char  ADCTEST0;
volatile __xdata __at  0x61b6 unsigned char  ADCTEST1;
volatile __xdata __at  0x61b7 unsigned char  ADCTEST2;
volatile __xdata __at  0x61b8 unsigned char  MDMTEST0;
volatile __xdata __at  0x61b9 unsigned char  MDMTEST1;
volatile __xdata __at  0x61ba unsigned char  DACTEST0;
volatile __xdata __at  0x61bb unsigned char  DACTEST1;
volatile __xdata __at  0x61bc unsigned char  DACTEST2;
volatile __xdata __at  0x61bd unsigned char  ATEST;
volatile __xdata __at  0x61be unsigned char  PTEST0;
volatile __xdata __at  0x61bf unsigned char  PTEST1;
volatile __xdata __at  0x61e0 unsigned char  CSPCTRL;
volatile __xdata __at  0x61e1 unsigned char  CSPSTAT;
volatile __xdata __at  0x61e2 unsigned char  CSPX;
volatile __xdata __at  0x61e3 unsigned char  CSPY;
volatile __xdata __at  0x61eb unsigned char  RFC_OBS_CTRL0;
volatile __xdata __at  0x61ec unsigned char  RFC_OBS_CTRL1;
volatile __xdata __at  0x61ed unsigned char  RFC_OBS_CTRL2;
volatile __xdata __at  0x61fa unsigned char  TXFILTCFG;

void rf_init(void);

extern __xdata unsigned char tmp_packet[];
#endif
