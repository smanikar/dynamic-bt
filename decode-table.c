/*
 * Copyright (c) 2005, Johns Hopkins University and The EROS Group, LLC.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 *  * Neither the name of the Johns Hopkins University, nor the name
 *    of The EROS Group, LLC, nor the names of their contributors may
 *    be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Table-assisted decoder for the x86 instruction set.
 */

#include <stdbool.h>
#include "switches.h"
#include "debug.h"
#include "machine.h"
#include "decode.h"
#include "emit.h"

/* The following opcode table was obtained from the Appendix A "Opcode
 * Map" tables of the Pentium family architectural manuals. AMD has
 * added subsequent enhancements in defining the x86/64
 * architecture. Those are NOT reflected here, as we are not trying to
 * emulate an x86/64 at this time. They probably should be.
 *
 * For each opcode, all operands are described. Where an operand is an
 * "implied" operand, it is indicated with a leading lowercase letter.
 *
 * Codes for addressing method:
 *
 * A  direct address, encoded as an immediate in the instruction
 *    stream.
 * C  reg field of modR/M byte selects a control register.
 * D  reg field of modR/M byte selects a debug register
 * E  a modR/M byte follows opcode and specifies the operand. This may
 *    in turn imply a SIB byte.
 * F  flags register
 * G  the reg field of the modR/M byte selects a general register.
 * I  immediate data
 * J  offset relative to start of next instruction.
 * M  modR/M byte may only refer to memory (BOUND, LES, LDS, LSS, LFS,
 *    LGS, CMPXCHG8b)
 * O  The instruction has no modR/M byte; offset is coded as a word or
 *    double word depending on address size attribute.
 * R  The mod field of the modR/M byte may refer only to a general
 *    register. 
 * S  The reg field of the modR/M byte selects a segment register
 * T  The reg field of the modR/M byte selects a test register
 * X  Memory addresses by the DS:SI register pair.
 * Y  Memory addresses by the ES:DI register pair.
 *
 * Codes for operand type:
 *
 * a  Two one word operands in memory or two double word operands in
 *    memory, depending on operand size attribute (BOUND only)
 * b  Byte (regardless of operand size attribute)
 * c  Byte or word, depending on operand size attribute
 * d  Double word (regardless of operand size attribute)
 * p  32-bit or 48-bit pointer, depending on operand size attribute
 * q  Quad word (regardless of operand size attribute)
 * s  Six-byte pseudo-descriptor
 * v  Word or double word, depending on operand size attribute
 * w  word (regardless of operand size attribute)
 *
 * Other encodings:
 *
 * 1. register name in all caps indicates that exact register of that
 *    exact size.
 * 2. three letter name with leading letter downcased (eAX) indicates
 *    either word or double word register according to operand size
 *
 * note that neither of these "operands" is ever explicitly encoded!
 */
/* FIX: Check Ib vs sIB */
/* After building my original decoder, I looked at the GDB decoder to
 * see how they handled suffixes. The idea of using capital letters to
 * signal suffixes was elegant, so I adopted it here. This adoption
 * was NOT done by copying. 
 *
 * I independently had used the same capture structure for opcode
 * arguments. After getting everything keyed I compared the result to
 * GDB's i386-opcode.c to check for errors.
 *
 * B -- trailing 'b' suffix
 * L -- trailing 'w' or 'l' suffix according to mode. 
 * Q -- trailinq 'd' or 'q' suffix according to mode
 * W -- b or w (cbw/cwd) according to mode
 * R -- w or d (cbw/cwd) according to mode
 * N -- 16 or 32 according to mode (inverted -- for size prefixes)
 */

#define AH  ADDR_implied_reg, reg_AH
#define AL  ADDR_implied_reg, reg_AL
#define BH  ADDR_implied_reg, reg_BH
#define BL  ADDR_implied_reg, reg_BL
#define CH  ADDR_implied_reg, reg_CH
#define CL  ADDR_implied_reg, reg_CL
#define DH  ADDR_implied_reg, reg_DH
#define DL  ADDR_implied_reg, reg_DL
#define DX  ADDR_implied_reg, reg_DX
#define indirDX  ADDR_implied_reg, reg_indirDX

#define eAX ADDR_implied_reg, reg_EAX
#define eBP ADDR_implied_reg, reg_EBP
#define eBX ADDR_implied_reg, reg_EBX
#define eCX ADDR_implied_reg, reg_ECX
#define eDI ADDR_implied_reg, reg_EDI
#define eDX ADDR_implied_reg, reg_EDX
#define eSI ADDR_implied_reg, reg_ESI
#define eSP ADDR_implied_reg, reg_ESP

#define EAX ADDR_implied_reg, reg_EAX
#define EBP ADDR_implied_reg, reg_EBP
#define EBX ADDR_implied_reg, reg_EBX
#define ECX ADDR_implied_reg, reg_ECX
#define EDI ADDR_implied_reg, reg_EDI
#define EDX ADDR_implied_reg, reg_EDX
#define ESI ADDR_implied_reg, reg_ESI
#define ESP ADDR_implied_reg, reg_ESP

#define CS  ADDR_seg, reg_CS
#define DS  ADDR_seg, reg_DS
#define ES  ADDR_seg, reg_ES
#define FS  ADDR_seg, reg_FS
#define GS  ADDR_seg, reg_GS
#define SS  ADDR_seg, reg_SS

#define Ap  ADDR_direct, p_mode
#define Eb  ADDR_E, b_mode
#define Ev  ADDR_E, v_mode
#define Ep  ADDR_E, p_mode
#define Ew  ADDR_E, w_mode
#define Gb  ADDR_G, b_mode
#define Gv  ADDR_G, v_mode
#define Gw  ADDR_G, w_mode
#define Ib  ADDR_imm, b_mode
#define Iv  ADDR_imm, v_mode
#define Iw  ADDR_imm, w_mode
#define Jb  ADDR_jmp, b_mode
#define Jv  ADDR_jmp, v_mode
#define M   ADDR_E, 0
#define Ma  ADDR_E, v_mode
#define Mb  ADDR_E, b_mode
#define Md  ADDR_E, d_mode
#define Mp  ADDR_E, p_mode
#define Mq  ADDR_E, q_mode
#define Ms  ADDR_E, s_mode
#define Mw  ADDR_E, w_mode
#define Mx  ADDR_E, x_mode
#define My  ADDR_E, y_mode
#define Mz  ADDR_E, z_mode
#define Ob  ADDR_offset, b_mode
#define Ov  ADDR_offset, v_mode
#define Rd  ADDR_R, d_mode		/* ?? */
#define Cd  ADDR_C, d_mode		/* ?? */
#define Dd  ADDR_D, d_mode		/* ?? */
#define Td  ADDR_T, d_mode		/* ?? */
#define Sw  ADDR_seg_reg, w_mode
#define Xb  ADDR_ds, reg_ESI
#define Xv  ADDR_ds, reg_ESI
#define Yb  ADDR_es, reg_EDI
#define Yv  ADDR_es, reg_EDI

#define FREG ADDR_FREG, 0

#define NONE 0, 0

#define PREFIX(s) s, NONE, NONE, NONE, 0, "(prefix)", DF_PREFIX
#define RESERVED  "(reserved)", NONE, NONE, NONE, 0, "(reserved)", DF_UNDEFINED
#define GROUP(nm) "(group)", NONE, NONE, NONE, (const void *)nm, #nm, DF_GROUP
#define TABLE(nm) "(table)", NONE, NONE, NONE, (const void *)nm, #nm, DF_TABLE
#define FLOAT(nm) "(float)", NONE, NONE, NONE, (const void *)nm, #nm, DF_FLOAT
#define FTABLE(nm) "(float)", NONE, NONE, NONE, (const void *)nm, #nm, DF_ONE_MORE_LEVEL

#define EMIT(nm) emit_##nm, "emit_" #nm

#define XX	0x0u
#define AX	DF_ADDRSZ_PREFIX
#define XO	DF_OPSZ_PREFIX
#define AO	DF_ADDRSZ_PREFIX | DF_OPSZ_PREFIX

#define JJ      DF_BRANCH

/* Support for MMX Technology, SSE Extensions and SSE2 Extensions 
   Instruction Set Enhancements: (For Now!) */
#define MMX_SSE_SSE2 		"MMX/SSE/SSE2", M,    NONE, NONE, EMIT(normal), AX, NF
#define MMX_SSE_SSE2_Ib 	"MMX/SSE/SSE2", M,    Ib,   NONE, EMIT(normal), AX, NF
#define MMX_SSE_SSE2_None 	"MMX/SSE/SSE2", NONE, NONE, NONE, EMIT(normal), AX, NF

const
OpCode nopbyte0[256] = {
  { 0x00u,  "addB",   Eb,   Gb,   NONE, EMIT(normal), 	AX, WF },
  { 0x01u,  "addL",   Ev,   Gv,   NONE, EMIT(normal), 	AO, WF },
  { 0x02u,  "addB",   Gb,   Eb,   NONE, EMIT(normal), 	AX, WF },
  { 0x03u,  "addL",   Gv,   Ev,   NONE, EMIT(normal), 	AO, WF },
  { 0x04u,  "addB",   AL,   Ib,   NONE, EMIT(normal), 	XX, WF },
  { 0x05u,  "addL",   eAX,  Iv,   NONE, EMIT(normal), 	XO, WF },
  { 0x06u,  "push",   ES,   NONE, NONE, EMIT(normal), 	XO, NF },
  { 0x07u,  "pop",    ES,   NONE, NONE, EMIT(normal), 	XO, NF },
  /* 0x08 */
  { 0x08u,  "orB",    Eb,   Gb,   NONE, EMIT(normal), 	AX, WF },
  { 0x09u,  "orL",    Ev,   Gv,   NONE, EMIT(normal), 	AO, WF },
  { 0x0au,  "orB",    Gb,   Eb,   NONE, EMIT(normal), 	AX, WF },
  { 0x0bu,  "orL",    Gv,   Ev,   NONE, EMIT(normal), 	AO, WF },
  { 0x0cu,  "orB",    AL,   Ib,   NONE, EMIT(normal), 	XX, WF },
  { 0x0du,  "orL",    eAX,  Iv,   NONE, EMIT(normal), 	XO, WF },
  { 0x0eu,  "push",   CS,   NONE, NONE, EMIT(normal), 	XO, NF },
  /* 0x0f is a two-byte opcode escape */
  { 0x0fu, TABLE(twoByteOpcodes) },
  /* 0x10 */
  { 0x10u,  "adcB",   Eb,   Gb,   NONE, EMIT(normal), 	AX, RWF },
  { 0x11u,  "adcL",   Ev,   Gv,   NONE, EMIT(normal), 	AO, RWF },
  { 0x12u,  "adcB",   Gb,   Eb,   NONE, EMIT(normal), 	AX, RWF },
  { 0x13u,  "adcL",   Gv,   Ev,   NONE, EMIT(normal), 	AO, RWF },
  { 0x14u,  "adcB",   AL,   Ib,   NONE, EMIT(normal), 	XX, RWF },
  { 0x15u,  "adcL",   eAX,  Iv,   NONE, EMIT(normal), 	XO, RWF },
  { 0x16u,  "push",   SS,   NONE, NONE, EMIT(normal), 	XO, NF },
  { 0x17u,  "pop",    SS,   NONE, NONE, EMIT(normal), 	XO, NF },
  /* 0x18 */
  { 0x18u,  "sbbB",   Eb,   Gb,   NONE, EMIT(normal),   AX, RWF },
  { 0x19u,  "sbbL",   Ev,   Gv,   NONE, EMIT(normal),	AO, RWF },
  { 0x1au,  "sbbB",   Gb,   Eb,   NONE, EMIT(normal),	AX, RWF },
  { 0x1bu,  "sbbL",   Gv,   Ev,   NONE, EMIT(normal),	AO, RWF },
  { 0x1cu,  "sbbB",   AL,   Ib,   NONE, EMIT(normal),	XX, RWF },
  { 0x1du,  "sbbL",   eAX,  Iv,   NONE, EMIT(normal),	XO, RWF },
  { 0x1eu,  "push",   DS,   NONE, NONE, EMIT(normal),	XO, NF },
  { 0x1fu,  "pop",    DS,   NONE, NONE, EMIT(normal),	XO, NF },
  /* 0x20 */
  { 0x20u,  "andB",   Eb,   Gb,   NONE, EMIT(normal),	AX, WF },
  { 0x21u,  "andL",   Ev,   Gv,   NONE, EMIT(normal),	AO, WF },
  { 0x22u,  "andB",   Gb,   Eb,   NONE, EMIT(normal),	AX, WF },
  { 0x23u,  "andL",   Gv,   Ev,   NONE, EMIT(normal),	AO, WF },
  { 0x24u,  "andB",   AL,   Ib,   NONE, EMIT(normal),	XX, WF },
  { 0x25u,  "andL",   eAX,  Iv,   NONE, EMIT(normal),	XO, WF },
  { 0x26u, PREFIX("es") },
  { 0x27u,  "daa",    NONE, NONE, NONE, EMIT(normal),	XX, RWF },
  /* 0x28 */
  { 0x28u,  "subB",   Eb,   Gb,   NONE, EMIT(normal),	AX, WF },
  { 0x29u,  "subL",   Ev,   Gv,   NONE, EMIT(normal),	AO, WF },
  { 0x2au,  "subB",   Gb,   Eb,   NONE, EMIT(normal),	AX, WF },
  { 0x2bu,  "subL",   Gv,   Ev,   NONE, EMIT(normal), 	AO, WF },
  { 0x2cu,  "subB",   AL,   Ib,   NONE, EMIT(normal), 	XX, WF },
  { 0x2du,  "subL",   eAX,  Iv,   NONE, EMIT(normal),  	XO, WF },
  { 0x2eu, PREFIX("cs") },
  { 0x2fu,  "das",    NONE, NONE, NONE, EMIT(normal), 	XX, RWF },
  /* 0x30 */
  { 0x30u,  "xorB",   Eb,   Gb,   NONE, EMIT(normal), 	AX, WF },
  { 0x31u,  "xorL",   Ev,   Gv,   NONE, EMIT(normal), 	AO, WF },
  { 0x32u,  "xorB",   Gb,   Eb,   NONE, EMIT(normal),	AX, WF },
  { 0x33u,  "xorL",   Gv,   Ev,   NONE, EMIT(normal),	AO, WF },
  { 0x34u,  "xorB",   AL,   Ib,   NONE, EMIT(normal),	XX, WF },
  { 0x35u,  "xorL",   eAX,  Iv,   NONE, EMIT(normal),	XO, WF },
  { 0x36u, PREFIX("ss") },
  { 0x37u,  "aaa",    NONE, NONE, NONE, EMIT(normal),	XX , RWF },
  /* 0x38 */
  { 0x38u,  "cmpB",   Eb,   Gb,   NONE, EMIT(normal),	AX, WF },
  { 0x39u,  "cmpL",   Ev,   Gv,   NONE, EMIT(normal),	AO, WF },
  { 0x3au,  "cmpB",   Gb,   Eb,   NONE, EMIT(normal),	AX, WF },
  { 0x3bu,  "cmpL",   Gv,   Ev,   NONE, EMIT(normal),	AO, WF },
  { 0x3cu,  "cmpB",   AL,   Ib,   NONE, EMIT(normal),	XX, WF },
  { 0x3du,  "cmpL",   eAX,  Iv,   NONE, EMIT(normal),	XO, WF },
  { 0x3eu, PREFIX("ds") },
  { 0x3fu,  "aas",    NONE, NONE, NONE, EMIT(normal),   XX , RWF },  
  /* 0x40 */
  { 0x40u,  "incL",   eAX,  NONE, NONE, EMIT(normal),	XO, WAOF },
  { 0x41u,  "incL",   eCX,  NONE, NONE, EMIT(normal),	XO, WAOF },
  { 0x42u,  "incL",   eDX,  NONE, NONE, EMIT(normal),	XO, WAOF },
  { 0x43u,  "incL",   eBX,  NONE, NONE, EMIT(normal),	XO, WAOF },
  { 0x44u,  "incL",   eSP,  NONE, NONE, EMIT(normal),	XO, WAOF },
  { 0x45u,  "incL",   eBP,  NONE, NONE, EMIT(normal),	XO, WAOF },
  { 0x46u,  "incL",   eSI,  NONE, NONE, EMIT(normal),	XO, WAOF },
  { 0x47u,  "incL",   eDI,  NONE, NONE, EMIT(normal),	XO, WAOF },
  /* 0x48 */
  { 0x48u,  "decL",   eAX,  NONE, NONE, EMIT(normal),	XO, WAOF },
  { 0x49u,  "decL",   eCX,  NONE, NONE, EMIT(normal),	XO, WAOF },
  { 0x4au,  "decL",   eDX,  NONE, NONE, EMIT(normal),	XO, WAOF },
  { 0x4bu,  "decL",   eBX,  NONE, NONE, EMIT(normal),	XO, WAOF },
  { 0x4cu,  "decL",   eSP,  NONE, NONE, EMIT(normal),	XO, WAOF },
  { 0x4du,  "decL",   eBP,  NONE, NONE, EMIT(normal),	XO, WAOF },
  { 0x4eu,  "decL",   eSI,  NONE, NONE, EMIT(normal),	XO, WAOF },
  { 0x4fu,  "decL",   eDI,  NONE, NONE, EMIT(normal),	XO, WAOF },
  /* 0x50 */
  { 0x50u,  "pushL",  eAX,  NONE, NONE, EMIT(normal),	XO, NF },
  { 0x51u,  "pushL",  eCX,  NONE, NONE, EMIT(normal),	XO, NF },
  { 0x52u,  "pushL",  eDX,  NONE, NONE, EMIT(normal),	XO, NF },
  { 0x53u,  "pushL",  eBX,  NONE, NONE, EMIT(normal),	XO, NF },
  { 0x54u,  "pushL",  eSP,  NONE, NONE, EMIT(normal),	XO, NF },
  { 0x55u,  "pushL",  eBP,  NONE, NONE, EMIT(normal),	XO, NF },
  { 0x56u,  "pushL",  eSI,  NONE, NONE, EMIT(normal),	XO, NF },
  { 0x57u,  "pushL",  eDI,  NONE, NONE, EMIT(normal),	XO, NF },
  /* 0x58 */
  { 0x58u,  "popL",   eAX,  NONE, NONE, EMIT(normal),	XO, NF },
  { 0x59u,  "popL",   eCX,  NONE, NONE, EMIT(normal),  	XO, NF },
  { 0x5au,  "popL",   eDX,  NONE, NONE, EMIT(normal), 	XO, NF },
  { 0x5bu,  "popL",   eBX,  NONE, NONE, EMIT(normal),	XO, NF },
  { 0x5cu,  "popL",   eSP,  NONE, NONE, EMIT(normal),	XO, NF },
  { 0x5du,  "popL",   eBP,  NONE, NONE, EMIT(normal),	XO, NF },
  { 0x5eu,  "popL",   eSI,  NONE, NONE, EMIT(normal),	XO, NF },
  { 0x5fu,  "popL",   eDI,  NONE, NONE, EMIT(normal),	XO, NF },
  /* 0x60 */
  { 0x60u,  "pushaL", NONE, NONE, NONE, EMIT(normal),	XO, NF },
  { 0x61u,  "popaL",  NONE, NONE, NONE, EMIT(normal),	XO, NF },
  { 0x62u,  "bound",  Gv,   Ma,   NONE, EMIT(normal),	AO, NF },
  { 0x63u,  "arpl",   Ew,   Gw,   NONE, EMIT(normal),	AX, WPF },
  { 0x64u, PREFIX("fs") },
  { 0x65u, PREFIX("gs") },
  { 0x66u, PREFIX("dataN") },	/* Operand size override prefix */
  { 0x67u, PREFIX("addrN") },	/* Address size override prefix */
  { 0x68u,  "pushL",  Iv,   NONE, NONE, EMIT(normal),	XO, NF },
  { 0x69u,  "imulL",  Gv,   Ev,   Iv,   EMIT(normal), 	AO, WF },
  { 0x6au,  "push",   Ib,   NONE, NONE, EMIT(normal),	XX, NF }, /* sign extended */
  { 0x6bu,  "imulL",  Gv,   Ev,   Ib,   EMIT(normal),	AO, WF }, /* sign extended */
  { 0x6cu,  "insB",   Yb,   indirDX,   NONE, EMIT(normal), AX, RF },
  { 0x6du,  "insL",   Yv,   indirDX,   NONE, EMIT(normal), AO, RF },
  { 0x6eu,  "outsB",  indirDX,   Xb,   NONE, EMIT(normal), AX, RF },
  { 0x6fu,  "outsL",  indirDX,   Xv,   NONE, EMIT(normal), AO, RF },
  /* 0x70 */
  { 0x70u, "jo",      Jb,   NONE, NONE, EMIT(jcond),	XO | JJ, RF },
  { 0x71u, "jno",     Jb,   NONE, NONE, EMIT(jcond),	XO | JJ, RF },
  { 0x72u, "jb",      Jb,   NONE, NONE, EMIT(jcond), 	XO | JJ, RF },
  { 0x73u, "jae",     Jb,   NONE, NONE, EMIT(jcond),	XO | JJ, RF },
  { 0x74u, "je",      Jb,   NONE, NONE, EMIT(jcond),	XO | JJ, RF },
  { 0x75u, "jne",     Jb,   NONE, NONE, EMIT(jcond),	XO | JJ, RF },
  { 0x76u, "jbe",     Jb,   NONE, NONE, EMIT(jcond),	XO | JJ, RF },
  { 0x77u, "ja",      Jb,   NONE, NONE, EMIT(jcond),	XO | JJ, RF },
  /* 0x78 */
  { 0x78u, "js",      Jb,   NONE, NONE, EMIT(jcond),	XO | JJ, RF },
  { 0x79u, "jns",     Jb,   NONE, NONE, EMIT(jcond),	XO | JJ, RF },
  { 0x7au, "jp",      Jb,   NONE, NONE, EMIT(jcond),	XO | JJ, RF },
  { 0x7bu, "jnp",     Jb,   NONE, NONE, EMIT(jcond),	XO | JJ, RF },
  { 0x7cu, "jl",      Jb,   NONE, NONE, EMIT(jcond),	XO | JJ, RF },
  { 0x7du, "jge",     Jb,   NONE, NONE, EMIT(jcond),	XO | JJ, RF },
  { 0x7eu, "jle",     Jb,   NONE, NONE, EMIT(jcond),	XO | JJ, RF },
  { 0x7fu, "jg",      Jb,   NONE, NONE, EMIT(jcond), 	XO | JJ, RF },
  /* 0x80 */
  { 0x80u, GROUP(group1_Eb_Ib) },
  { 0x81u, GROUP(group1_Ev_Iv) },
  { 0x82u, GROUP(group1_Eb_Ib) },
  { 0x83u, GROUP(group1_Ev_Ib) },
  { 0x84u, "testB",   Eb,   Gb,   NONE, EMIT(normal),	AX, WF },
  { 0x85u, "testL",   Ev,   Gv,   NONE, EMIT(normal),	AO, WF },
  { 0x86u, "xchgB",   Eb,   Gb,   NONE, EMIT(normal),	AX, NF },
  { 0x87u, "xchgL",   Ev,   Gv,   NONE, EMIT(normal),	AO, NF },
  /* 0x88 */
  { 0x88u, "movB",    Eb,   Gb,   NONE, EMIT(normal),	AX, NF },
  { 0x89u, "movL",    Ev,   Gv,   NONE, EMIT(normal),	AO, NF },
  { 0x8au, "movB",    Gb,   Eb,   NONE, EMIT(normal), 	AX, NF },
  { 0x8bu, "movL",    Gv,   Ev,   NONE, EMIT(normal),	AO, NF },
  { 0x8cu, "movL",    Ew,   Sw,   NONE, EMIT(normal),	AO, NF },
  { 0x8du, "lea",     Gv,   M,    NONE, EMIT(normal),	AO, NF },
  { 0x8eu, "movL",    Sw,   Ew,   NONE, EMIT(normal),	AO, NF },
  { 0x8fu, "popL",    Ev,   NONE, NONE, EMIT(normal),	AO, NF },
  /* 0x90 */
  { 0x90u, "nop",     NONE, NONE, NONE, EMIT(normal),	XO, NF },
  { 0x91u, "xchgL",   eCX,  eAX,  NONE, EMIT(normal),	XO, NF },
  { 0x92u, "xchgL",   eDX,  eAX,  NONE, EMIT(normal),	XO, NF },
  { 0x93u, "xchgL",   eBX,  eAX,  NONE, EMIT(normal),	XO, NF },
  { 0x94u, "xchgL",   eSP,  eAX,  NONE, EMIT(normal),	XO, NF },
  { 0x95u, "xchgL",   eBP,  eAX,  NONE, EMIT(normal),	XO, NF },
  { 0x96u, "xchgL",   eSI,  eAX,  NONE, EMIT(normal),	XO, NF },
  { 0x97u, "xchgL",   eDI,  eAX,  NONE, EMIT(normal),	XO, NF },
  /* 0x98 */
  { 0x98u, "cWD",     NONE, NONE, NONE, EMIT(normal),	XO, NF },
  { 0x99u, "cDQ",     NONE, NONE, NONE, EMIT(normal),	XO, NF },
  { 0x9au, "call",    Ap,   NONE, NONE, EMIT(normal),	XO, NF },
  { 0x9bu, "wait",    NONE, NONE, NONE, EMIT(normal),	XX, NF },
  { 0x9cu, "pushf",   NONE, NONE, NONE, EMIT(normal),	XO, RF },
  { 0x9du, "popf",    NONE, NONE, NONE, EMIT(normal),	XO, WF },
  { 0x9eu, "sahf",    NONE, NONE, NONE, EMIT(normal),	XX, WF },
  { 0x9fu, "lahf",    NONE, NONE, NONE, EMIT(normal),	XX, NF },
  /* 0xa0 */
  { 0xa0u, "movB",    AL,   Ob,   NONE, EMIT(normal),	AX, NF },
  { 0xa1u, "movL",    eAX,  Ov,   NONE, EMIT(normal),	AO, NF },
  { 0xa2u, "movB",    Ob,   AL,   NONE, EMIT(normal),	AX, NF },
  { 0xa3u, "movL",    Ov,   eAX,  NONE, EMIT(normal),	AO, NF },
  { 0xa4u, "movsB",   Yb,   Xb,   NONE, EMIT(normal),	AX, RF },
  { 0xa5u, "movsL",   Yv,   Xv,   NONE, EMIT(normal),	AO, RF },
  { 0xa6u, "cmpsB",   Xb,   Yb,   NONE, EMIT(normal),	AX, WF },
  { 0xa7u, "cmpsL",   Xv,   Yv,   NONE, EMIT(normal),	AO, WF },
  /* 0xa8 */
  { 0xa8u, "testB",   AL,   Ib,   NONE, EMIT(normal),	XX, WF },
  { 0xa9u, "testL",   eAX,  Iv,   NONE, EMIT(normal),	XO, WF },
  { 0xaau, "stosB",   Yb,   AL,   NONE, EMIT(normal),	AX, RF },
  { 0xabu, "stosL",   Yv,   eAX,  NONE, EMIT(normal),	AO, RF },
  { 0xacu, "lodsB",   AL,   Xb,   NONE, EMIT(normal),	AX, NF },
  { 0xadu, "lodsL",   eAX,  Xv,   NONE, EMIT(normal),	AO, NF },
  { 0xaeu, "scasB",   AL,   Yb,   NONE, EMIT(normal),	AX, RWF },
  { 0xafu, "scasL",   eAX,  Yv,   NONE, EMIT(normal),	AO, RWF },
  /* 0xb0 */
  { 0xb0u, "movB",    AL,   Ib,   NONE, EMIT(normal),	XX, NF },
  { 0xb1u, "movB",    CL,   Ib,   NONE, EMIT(normal),	XX, NF },
  { 0xb2u, "movB",    DL,   Ib,   NONE, EMIT(normal),	XX, NF },
  { 0xb3u, "movB",    BL,   Ib,   NONE, EMIT(normal),	XX, NF },
  { 0xb4u, "movB",    AH,   Ib,   NONE, EMIT(normal),	XX, NF },
  { 0xb5u, "movB",    CH,   Ib,   NONE, EMIT(normal),	XX, NF },
  { 0xb6u, "movB",    DH,   Ib,   NONE, EMIT(normal),	XX, NF },
  { 0xb7u, "movB",    BH,   Ib,   NONE, EMIT(normal),	XX, NF },
  /* 0xb8 */
  { 0xb8u, "movL",    eAX,  Iv,   NONE, EMIT(normal),	XO, NF },
  { 0xb9u, "movL",    eCX,  Iv,   NONE, EMIT(normal),	XO, NF },
  { 0xbau, "movL",    eDX,  Iv,   NONE, EMIT(normal),	XO, NF },
  { 0xbbu, "movL",    eBX,  Iv,   NONE, EMIT(normal),	XO, NF },
  { 0xbcu, "movL",    eSP,  Iv,   NONE, EMIT(normal),	XO, NF },
  { 0xbdu, "movL",    eBP,  Iv,   NONE, EMIT(normal),	XO, NF },
  { 0xbeu, "movL",    eSI,  Iv,   NONE, EMIT(normal),	XO, NF },
  { 0xbfu, "movL",    eDI,  Iv,   NONE, EMIT(normal),	XO, NF },
  /* 0xc0 */
  { 0xc0u, GROUP(group2a_Eb_Ib) },
  { 0xc1u, GROUP(group2a_Ev_Ib) },
  { 0xc2u, "retL",    Iw,   NONE, NONE, EMIT(ret_Iw),	XO | JJ, NF },
  { 0xc3u, "retL",    NONE, NONE, NONE, EMIT(ret),	XO | JJ, NF },
  { 0xc4u, "les",     Gv,   Mp,   NONE, EMIT(normal), 	AO, NF },
  { 0xc5u, "lds",     Gv,   Mp,   NONE, EMIT(normal),	AO, NF },
  { 0xc6u, "movB",    Eb,   Ib,   NONE, EMIT(normal),	AX, NF },
  { 0xc7u, "movL",    Ev,   Iv,   NONE, EMIT(normal),	AO, NF },
  /* 0xc8 */
  { 0xc8u, "enter",   Iw,   Ib,   NONE, EMIT(normal),	XX, NF },
  { 0xc9u, "leaveL",  NONE, NONE, NONE, EMIT(normal),	XO, NF },
  { 0xcau, "lret",    Iw,   NONE, NONE, EMIT(normal),	XO, NF }, /* ret far */
  { 0xcbu, "lret",    NONE, NONE, NONE, EMIT(normal),	XO, NF }, /* ret far */
  { 0xccu, "int3",    NONE, NONE, NONE, EMIT(normal),	XX, RWF },
  { 0xcdu, "int",     Ib,   NONE, NONE, EMIT(int),	XX, RWF },
  { 0xceu, "into",    NONE, NONE, NONE, EMIT(normal),	XX, RWF },
  { 0xcfu, "iret",    NONE, NONE, NONE, EMIT(normal),	XO, RWF },
  /* 0xd0 */
  { 0xd0u, GROUP(group2_Eb_1)  },
  { 0xd1u, GROUP(group2_Ev_1)  },
  { 0xd2u, GROUP(group2_Eb_CL) },
  { 0xd3u, GROUP(group2_Ev_CL) },
  { 0xd4u, "aam",     Ib,   NONE, NONE, EMIT(normal),	XX , WF },
  { 0xd5u, "aad",     Ib,   NONE, NONE, EMIT(normal),	XX , WF },
  { 0xd6u, RESERVED },
  { 0xd7u, "xlat",    NONE, NONE, NONE, EMIT(normal),	AX, NF },
  /* 0xd8 */
  { 0xd8u, FLOAT(float_d8) }, /* coprocessor opcodes */
  { 0xd9u, FLOAT(float_d9) }, /* coprocessor opcodes */
  { 0xdau, FLOAT(float_da) }, /* coprocessor opcodes */
  { 0xdbu, FLOAT(float_db) }, /* coprocessor opcodes */
  { 0xdcu, FLOAT(float_dc) }, /* coprocessor opcodes */
  { 0xddu, FLOAT(float_dd) }, /* coprocessor opcodes */
  { 0xdeu, FLOAT(float_de) }, /* coprocessor opcodes */
  { 0xdfu, FLOAT(float_df) }, /* coprocessor opcodes */
  /* 0xe0 */
  { 0xe0u, "loopne",  Jb,   NONE, NONE, EMIT(other_jcond), AO | JJ, RF },
  { 0xe1u, "loope",   Jb,   NONE, NONE, EMIT(other_jcond), AO | JJ, RF },
  { 0xe2u, "loop",    Jb,   NONE, NONE, EMIT(other_jcond), AO | JJ, NF },
  { 0xe3u, "jcxz",    Jb,   NONE, NONE, EMIT(other_jcond), AO | JJ, NF },
  { 0xe4u, "inB",     AL,   Ib,   NONE, EMIT(normal), 	XX, NF },
  { 0xe5u, "inL",     eAX,  Ib,   NONE, EMIT(normal),	XO, NF },
  { 0xe6u, "outB",    Ib,   AL,   NONE, EMIT(normal),	XX, NF },
  { 0xe7u, "outL",    Ib,   eAX,  NONE, EMIT(normal),	XO, NF },
  /* 0xe8 */
  { 0xe8u, "callL",   Jv,   NONE, NONE, EMIT(call_disp),  	XO | JJ, NF },
  { 0xe9u, "jmpL",    Jv,   NONE, NONE, EMIT(jmp),	 	XO | JJ, NF },
  { 0xeau, "ljmp",    Ap,   NONE, NONE, EMIT(normal),	 	XO, NF }, /* jmp far */
  { 0xebu, "jmp",     Jb,   NONE, NONE, EMIT(jmp),	 	XO | JJ, NF },
  { 0xecu, "inB",     AL,   indirDX,   NONE, EMIT(normal), 	XX, NF },
  { 0xedu, "inL",     eAX,  indirDX,   NONE, EMIT(normal),	XO, NF },
  { 0xeeu, "outB",    indirDX,   AL,   NONE, EMIT(normal),	XX, NF },
  { 0xefu, "outL",    indirDX,   eAX,  NONE, EMIT(normal),	XO, NF },
  /* 0xf0 */
  { 0xf0u, PREFIX("lock") },
  { 0xf1u, RESERVED },
  { 0xf2u, PREFIX("repne") },
  { 0xf3u, PREFIX("rep") },
  { 0xf4u, "hlt",     NONE, NONE, NONE, EMIT(normal),	XX, NF },
  { 0xf5u, "cmc",     NONE, NONE, NONE, EMIT(normal),	XX, RWCF },
  { 0xf6u, GROUP(group3b) },
  { 0xf7u, GROUP(group3v) },
  { 0xf8u, "clc",     NONE, NONE, NONE, EMIT(normal),	XX, WCF },
  { 0xf9u, "stc",     NONE, NONE, NONE, EMIT(normal),	XX, WF },
  { 0xfau, "cli",     NONE, NONE, NONE, EMIT(normal),	XX, WF },
  { 0xfbu, "sti",     NONE, NONE, NONE, EMIT(normal),	XX, WF },
  { 0xfcu, "cld",     NONE, NONE, NONE, EMIT(normal),	XX, WF },
  { 0xfdu, "std",     NONE, NONE, NONE, EMIT(normal),	XX, WF },
  { 0xfeu, GROUP(ngroup4), },
  { 0xffu, GROUP(ngroup5), }
};

const
OpCode twoByteOpcodes[256] = {
  /* 0x00 */
  { 0x00u, GROUP(ngroup6) },
  { 0x01u, GROUP(ngroup7) },
  { 0x02u, "lar",     Gv,   Ev,   NONE, EMIT(normal),	AO, WPF },
  { 0x03u, "lsl",     Gv,   Ev,   NONE, EMIT(normal),	AO, WPF },
  { 0x04u, RESERVED },
  { 0x05u, RESERVED },
  { 0x06u, "clts",     NONE, NONE, NONE, EMIT(normal),	XX, NF },
  { 0x07u, RESERVED },
  /* 0x08 */
  { 0x08u, "invd",     NONE, NONE, NONE, EMIT(normal),	XX, NF },
  { 0x09u, "wbinvd",   NONE, NONE, NONE, EMIT(normal),	XX, NF },
  { 0x0au, RESERVED },
  { 0x0bu, RESERVED },
  { 0x0cu, RESERVED },
  { 0x0du, RESERVED },
  { 0x0eu, RESERVED },
  { 0x0fu, RESERVED },
  /* 0x10 */
  { 0x10u, MMX_SSE_SSE2 },
  { 0x11u, MMX_SSE_SSE2 },
  { 0x12u, MMX_SSE_SSE2 },
  { 0x13u, MMX_SSE_SSE2 },
  { 0x14u, MMX_SSE_SSE2 },
  { 0x15u, MMX_SSE_SSE2 },
  { 0x16u, MMX_SSE_SSE2 },
  { 0x17u, MMX_SSE_SSE2 },
  /* 0x18 */
  { 0x18u, MMX_SSE_SSE2 },
  { 0x19u, RESERVED },
  { 0x1au, RESERVED },
  { 0x1bu, RESERVED },
  { 0x1cu, RESERVED },
  { 0x1du, RESERVED },
  { 0x1eu, RESERVED },
  { 0x1fu, RESERVED },
  /* 0x20 */
  { 0x20u, "mov",      Rd,   Cd,   NONE, EMIT(normal),	XX, NDF },
  { 0x21u, "mov",      Rd,   Dd,   NONE, EMIT(normal),	XX, NDF },
  { 0x22u, "mov",      Cd,   Rd,   NONE, EMIT(normal),	XX, NDF },
  { 0x23u, "mov",      Dd,   Rd,   NONE, EMIT(normal),	XX, NDF },
  { 0x24u, "mov",      Rd,   Td,   NONE, EMIT(normal),  XX, NDF },		
  { 0x25u, RESERVED },
  { 0x26u, "mov",      Td,   Rd,   NONE, EMIT(normal),  XX, NDF },		
  { 0x27u, RESERVED },
  /* 0x28 */
  { 0x28u, MMX_SSE_SSE2 },
  { 0x29u, MMX_SSE_SSE2 },
  { 0x2au, MMX_SSE_SSE2 },
  { 0x2bu, MMX_SSE_SSE2 },
  { 0x2cu, MMX_SSE_SSE2 },
  { 0x2du, MMX_SSE_SSE2 },
  { 0x2eu, MMX_SSE_SSE2 },
  { 0x2fu, MMX_SSE_SSE2 },
  /* 0x30 */
  { 0x30u, "wrmsr",    NONE, NONE, NONE, EMIT(normal),	XX, NF },
  { 0x31u, "rdtsc",    NONE, NONE, NONE, EMIT(normal),	XX, NF },
  { 0x32u, "rdmsr",    NONE, NONE, NONE, EMIT(normal),	XX, NF },
  { 0x33u, "rdpmc",    NONE, NONE, NONE, EMIT(normal),  XX, NF },
  { 0x34u, "sysenter", NONE, NONE, NONE, EMIT(sysenter),XX, NF },
  { 0x35u, "sysexit",  NONE, NONE, NONE, EMIT(normal),	XX, NF },
  { 0x36u, RESERVED },
  { 0x37u, RESERVED },
  /* 0x38 */
  { 0x38u, RESERVED },
  { 0x39u, RESERVED },
  { 0x3au, RESERVED },
  { 0x3bu, RESERVED },
  { 0x3cu, "movnti",   Gv,   Ev,   NONE, EMIT(normal),  XX },
  { 0x3du, RESERVED },
  { 0x3eu, RESERVED },
  { 0x3fu, RESERVED },
  /* 0x40 */
  { 0x40u, "cmovo",    Gv,   Ev,   NONE, EMIT(normal),	AO, RF },
  { 0x41u, "cmovno",   Gv,   Ev,   NONE, EMIT(normal),	AO, RF },
  { 0x42u, "cmovnae",  Gv,   Ev,   NONE, EMIT(normal),	AO, RF },
  { 0x43u, "cmovae",   Gv,   Ev,   NONE, EMIT(normal),	AO, RF },
  { 0x44u, "cmove",    Gv,   Ev,   NONE, EMIT(normal),	AO, RF },
  { 0x45u, "cmovne",   Gv,   Ev,   NONE, EMIT(normal),	AO, RF },
  { 0x46u, "cmovbe",   Gv,   Ev,   NONE, EMIT(normal),	AO, RF },
  { 0x47u, "cmovnbe",  Gv,   Ev,   NONE, EMIT(normal),	AO, RF },
  /* 0x48 */
  { 0x48u, "cmovs",    Gv,   Ev,   NONE, EMIT(normal),	AO, RF },
  { 0x49u, "cmovns",   Gv,   Ev,   NONE, EMIT(normal),	AO, RF },
  { 0x4au, "cmovp",    Gv,   Ev,   NONE, EMIT(normal),	AO, RF },
  { 0x4bu, "cmovnp",   Gv,   Ev,   NONE, EMIT(normal),	AO, RF },
  { 0x4cu, "cmovl",    Gv,   Ev,   NONE, EMIT(normal),	AO, RF },
  { 0x4du, "cmovge",   Gv,   Ev,   NONE, EMIT(normal),	AO, RF },
  { 0x4eu, "cmovle",   Gv,   Ev,   NONE, EMIT(normal),	AO, RF },
  { 0x4fu, "cmovg",    Gv,   Ev,   NONE, EMIT(normal),	AO, RF },
  /* 0x50 */
  { 0x50u, MMX_SSE_SSE2 },
  { 0x51u, MMX_SSE_SSE2 },
  { 0x52u, MMX_SSE_SSE2 },
  { 0x53u, MMX_SSE_SSE2 },
  { 0x54u, MMX_SSE_SSE2 },
  { 0x55u, MMX_SSE_SSE2 },
  { 0x56u, MMX_SSE_SSE2 },
  { 0x57u, MMX_SSE_SSE2 },
  /* 0x58 */
  { 0x58u, MMX_SSE_SSE2 },
  { 0x59u, MMX_SSE_SSE2 },
  { 0x5au, MMX_SSE_SSE2 },
  { 0x5bu, MMX_SSE_SSE2 },
  { 0x5cu, MMX_SSE_SSE2 },
  { 0x5du, MMX_SSE_SSE2 },
  { 0x5eu, MMX_SSE_SSE2 },
  { 0x5fu, MMX_SSE_SSE2 },
  /* 0x60 */
  { 0x60u, MMX_SSE_SSE2 },
  { 0x61u, MMX_SSE_SSE2 },
  { 0x62u, MMX_SSE_SSE2 },
  { 0x63u, MMX_SSE_SSE2 },
  { 0x64u, MMX_SSE_SSE2 },
  { 0x65u, MMX_SSE_SSE2 },
  { 0x66u, MMX_SSE_SSE2 },
  { 0x67u, MMX_SSE_SSE2 },
  /* 0x68 */
  { 0x68u, MMX_SSE_SSE2 },
  { 0x69u, MMX_SSE_SSE2 },
  { 0x6au, MMX_SSE_SSE2 },
  { 0x6bu, MMX_SSE_SSE2 },
  { 0x6cu, MMX_SSE_SSE2 },
  { 0x6du, MMX_SSE_SSE2 },
  { 0x6eu, MMX_SSE_SSE2 },
  { 0x6fu, MMX_SSE_SSE2 },
  /* 0x70 */
  { 0x70u, MMX_SSE_SSE2_Ib },
  { 0x71u, MMX_SSE_SSE2_Ib },
  { 0x72u, MMX_SSE_SSE2_Ib },
  { 0x73u, MMX_SSE_SSE2_Ib },
  { 0x74u, MMX_SSE_SSE2 },
  { 0x75u, MMX_SSE_SSE2 },
  { 0x76u, MMX_SSE_SSE2 },
  { 0x77u, MMX_SSE_SSE2_None },
  /* 0x78 */
  { 0x78u, RESERVED },
  { 0x79u, RESERVED },
  { 0x7au, RESERVED },
  { 0x7bu, RESERVED },
  { 0x7cu, RESERVED },
  { 0x7du, RESERVED },
  { 0x7eu, MMX_SSE_SSE2 },
  { 0x7fu, MMX_SSE_SSE2 },
  /* 0x80 */
  { 0x80u, "jo",      Jv,   NONE, NONE, EMIT(jcond),	XO | JJ, RF },
  { 0x81u, "jno",     Jv,   NONE, NONE, EMIT(jcond),	XO | JJ, RF },
  { 0x82u, "jb",      Jv,   NONE, NONE, EMIT(jcond),	XO | JJ, RF },
  { 0x83u, "jae",     Jv,   NONE, NONE, EMIT(jcond),	XO | JJ, RF },
  { 0x84u, "je",      Jv,   NONE, NONE, EMIT(jcond),	XO | JJ, RF },
  { 0x85u, "jne",     Jv,   NONE, NONE, EMIT(jcond),	XO | JJ, RF },
  { 0x86u, "jbe",     Jv,   NONE, NONE, EMIT(jcond),	XO | JJ, RF },
  { 0x87u, "ja",      Jv,   NONE, NONE, EMIT(jcond),	XO | JJ, RF },
  /* 0x88 */
  { 0x88u, "js",      Jv,   NONE, NONE, EMIT(jcond),	XO | JJ, RF },
  { 0x89u, "jns",     Jv,   NONE, NONE, EMIT(jcond),	XO | JJ, RF },
  { 0x8au, "jp",      Jv,   NONE, NONE, EMIT(jcond),	XO | JJ, RF },
  { 0x8bu, "jnp",     Jv,   NONE, NONE, EMIT(jcond),	XO | JJ, RF },
  { 0x8cu, "jl",      Jv,   NONE, NONE, EMIT(jcond),	XO | JJ, RF },
  { 0x8du, "jge",     Jv,   NONE, NONE, EMIT(jcond),	XO | JJ, RF },
  { 0x8eu, "jle",     Jv,   NONE, NONE, EMIT(jcond),	XO | JJ, RF },
  { 0x8fu, "jg",      Jv,   NONE, NONE, EMIT(jcond),	XO | JJ, RF },
  /* 0x90 */
  { 0x90u, "setoB",    Eb,   NONE, NONE, EMIT(normal),	AX, RF },
  { 0x91u, "setnoB",   Eb,   NONE, NONE, EMIT(normal),	AX, RF },
  { 0x92u, "setbB",    Eb,   NONE, NONE, EMIT(normal),	AX, RF },
  { 0x93u, "setnbB",   Eb,   NONE, NONE, EMIT(normal),	AX, RF },
  { 0x94u, "setzB",    Eb,   NONE, NONE, EMIT(normal),	AX, RF },
  { 0x95u, "setnzB",   Eb,   NONE, NONE, EMIT(normal),	AX, RF },
  { 0x96u, "setbeB",   Eb,   NONE, NONE, EMIT(normal),	AX, RF },
  { 0x97u, "setnbeB",  Eb,   NONE, NONE, EMIT(normal),	AX, RF },
  /* 0x98 */
  { 0x98u, "setsB",    Eb,   NONE, NONE, EMIT(normal),	AX, RF },
  { 0x99u, "setnsB",   Eb,   NONE, NONE, EMIT(normal),	AX, RF },
  { 0x9au, "setpB",    Eb,   NONE, NONE, EMIT(normal),	AX, RF },
  { 0x9bu, "setnpB",   Eb,   NONE, NONE, EMIT(normal),	AX, RF },
  { 0x9cu, "setlB",    Eb,   NONE, NONE, EMIT(normal),	AX, RF },
  { 0x9du, "setnlB",   Eb,   NONE, NONE, EMIT(normal),	AX, RF },
  { 0x9eu, "setleB",   Eb,   NONE, NONE, EMIT(normal),	AX, RF },
  { 0x9fu, "setnleB",  Eb,   NONE, NONE, EMIT(normal),	AX, RF },
  /* 0xa0 */
  { 0xa0u, "push",     FS,   NONE, NONE, EMIT(normal),	XO, NF },
  { 0xa1u, "pop",      FS,   NONE, NONE, EMIT(normal),	XO, NF },
  { 0xa2u, "cpuid",    NONE, NONE, NONE, EMIT(normal),	XX, NF },
  { 0xa3u, "btL",      Ev,   Gv,   NONE, EMIT(normal),	AO, WF },
  { 0xa4u, "shldL",    Ev,   Gv,   Ib,   EMIT(normal),	AO, WF },
  { 0xa5u, "shldL",    Ev,   Gv,   CL,   EMIT(normal),	AO, WF },
  /* Following two opcodes were actually CMPXCHG on the A step
     Pentium, but it was a mistake */
  { 0xa6u, RESERVED },
  { 0xa7u, RESERVED },
  /* 0xa8 */
  { 0xa8u, "push",     GS,   NONE, NONE, EMIT(normal),	XO, NF },
  { 0xa9u, "pop",      GS,   NONE, NONE, EMIT(normal),	XO, NF },
  { 0xaau, "rsm",      NONE, NONE, NONE, EMIT(normal),	XX, WF },
  { 0xabu, "btsL",     Ev,   Gv,   NONE, EMIT(normal),	AO, WF },
  { 0xacu, "shrdL",    Ev,   Gv,   Ib,   EMIT(normal),	AO, WF },
  { 0xadu, "shrdL",    Ev,   Gv,   CL,   EMIT(normal),	AO, WF },
  { 0xaeu, MMX_SSE_SSE2 },
  { 0xafu, "imulL",    Gv,   Ev,   NONE, EMIT(normal),	AO, WF },
  /* 0xb0 */
  { 0xb0u, "cmpxchgB", Eb,   Gb,   NONE, EMIT(normal),	AX, WPF },
  { 0xb1u, "cmpxchgL", Ev,   Gv,   NONE, EMIT(normal),	AO, WF },
  { 0xb2u, "lss",      Gv,   Mp,   NONE, EMIT(normal),	AO, NF },
  { 0xb3u, "btr",      Ev,   Gv,   NONE, EMIT(normal),	AO, WF },
  { 0xb4u, "lfs",      Gv,   Mp,   NONE, EMIT(normal),	AO, NF },
  { 0xb5u, "lgs",      Gv,   Mp,   NONE, EMIT(normal),	AO, NF },
  { 0xb6u, "movzbL",   Gv,   Eb,   NONE, EMIT(normal),	AO, NF }, //#
  { 0xb7u, "movzwL",   Gv,   Ew,   NONE, EMIT(normal),	AX, NF }, //#
  /* 0xb8 */
  { 0xb8u, RESERVED },
  { 0xb9u, RESERVED },
  { 0xbau, GROUP(group8_Ev_Ib) },
  { 0xbbu, "btcL",     Ev,   Gv,   NONE, EMIT(normal),	AO, WF },
  { 0xbcu, "bsfL",     Gv,   Ev,   NONE, EMIT(normal),	AO, WF },
  { 0xbdu, "bsrL",     Gv,   Ev,   NONE, EMIT(normal),	AO, WF },
  { 0xbeu, "movsxL",   Gv,   Eb,   NONE, EMIT(normal),	AO, NF },
  { 0xbfu, "movsxL",   Gv,   Ew,   NONE, EMIT(normal),	AX, NF },
  /* 0xc0 */
  { 0xc0u, "xaddB",    Eb,   Gb,   NONE, EMIT(normal),	AX, WF },
  { 0xc1u, "xaddL",    Ev,   Gv,   NONE, EMIT(normal),	AO, WF },
  { 0xc2u, MMX_SSE_SSE2_Ib },
  { 0xc3u, MMX_SSE_SSE2 },
  { 0xc4u, MMX_SSE_SSE2_Ib },
  { 0xc5u, MMX_SSE_SSE2_Ib },
  { 0xc6u, MMX_SSE_SSE2_Ib },
  { 0xc7u, GROUP(ngroup9) },
  /* 0xc8 */
  { 0xc8u, "bswap",    EAX,  NONE, NONE, EMIT(normal),	XX, NF },
  { 0xc9u, "bswap",    ECX,  NONE, NONE, EMIT(normal),	XX, NF },
  { 0xcau, "bswap",    EDX,  NONE, NONE, EMIT(normal),	XX, NF },
  { 0xcbu, "bswap",    EBX,  NONE, NONE, EMIT(normal),	XX, NF },
  { 0xccu, "bswap",    ESP,  NONE, NONE, EMIT(normal),	XX, NF },
  { 0xcdu, "bswap",    EBP,  NONE, NONE, EMIT(normal),	XX, NF },
  { 0xceu, "bswap",    ESI,  NONE, NONE, EMIT(normal),	XX, NF },
  { 0xcfu, "bswap",    EDI,  NONE, NONE, EMIT(normal),	XX, NF },
  /* 0xd0 */
  { 0xd0u, RESERVED },
  { 0xd1u, MMX_SSE_SSE2 },
  { 0xd2u, MMX_SSE_SSE2 },
  { 0xd3u, MMX_SSE_SSE2 },
  { 0xd4u, MMX_SSE_SSE2 },
  { 0xd5u, MMX_SSE_SSE2 },
  { 0xd6u, MMX_SSE_SSE2 },
  { 0xd7u, MMX_SSE_SSE2 },
  /* 0xd8 */
  { 0xd8u, MMX_SSE_SSE2 },
  { 0xd9u, MMX_SSE_SSE2 },
  { 0xdau, MMX_SSE_SSE2 },
  { 0xdbu, MMX_SSE_SSE2 },
  { 0xdcu, MMX_SSE_SSE2 },
  { 0xddu, MMX_SSE_SSE2 },
  { 0xdeu, MMX_SSE_SSE2 },
  { 0xdfu, MMX_SSE_SSE2 },
  /* 0xe0 */
  { 0xe0u, MMX_SSE_SSE2 },
  { 0xe1u, MMX_SSE_SSE2 },
  { 0xe2u, MMX_SSE_SSE2 },
  { 0xe3u, MMX_SSE_SSE2 },
  { 0xe4u, MMX_SSE_SSE2 },
  { 0xe5u, MMX_SSE_SSE2 },
  { 0xe6u, MMX_SSE_SSE2 },
  { 0xe7u, MMX_SSE_SSE2 },
  /* 0xe8 */
  { 0xe8u, MMX_SSE_SSE2 },
  { 0xe9u, MMX_SSE_SSE2 },
  { 0xeau, MMX_SSE_SSE2 },
  { 0xebu, MMX_SSE_SSE2 },
  { 0xecu, MMX_SSE_SSE2 },
  { 0xedu, MMX_SSE_SSE2 },
  { 0xeeu, MMX_SSE_SSE2 },
  { 0xefu, MMX_SSE_SSE2 },
  /* 0xf0 */
  { 0xf0u, RESERVED },
  { 0xf1u, MMX_SSE_SSE2 },
  { 0xf2u, MMX_SSE_SSE2 },
  { 0xf3u, MMX_SSE_SSE2 },
  { 0xf4u, MMX_SSE_SSE2 },
  { 0xf5u, MMX_SSE_SSE2 },
  { 0xf6u, MMX_SSE_SSE2 },
  { 0xf7u, MMX_SSE_SSE2 },
  /* 0xf8 */
  { 0xf8u, MMX_SSE_SSE2 },
  { 0xf9u, MMX_SSE_SSE2 },
  { 0xfau, MMX_SSE_SSE2 },
  { 0xfbu, MMX_SSE_SSE2 },
  { 0xfcu, MMX_SSE_SSE2 },
  { 0xfdu, MMX_SSE_SSE2 },
  { 0xfeu, MMX_SSE_SSE2 },
  { 0xffu, RESERVED }
};

const
OpCode group1_Eb_Ib[8] = {
  { 0x00u, "addB",   Eb,   Ib,   NONE, EMIT(normal), AX, WF },
  { 0x01u, "orB",    Eb,   Ib,   NONE, EMIT(normal), AX, WF },
  { 0x02u, "adcB",   Eb,   Ib,   NONE, EMIT(normal), AX, RWF },
  { 0x03u, "sbbB",   Eb,   Ib,   NONE, EMIT(normal), AX, RWF },
  { 0x04u, "andB",   Eb,   Ib,   NONE, EMIT(normal), AX, WF },
  { 0x05u, "subB",   Eb,   Ib,   NONE, EMIT(normal), AX, WF },
  { 0x06u, "xorB",   Eb,   Ib,   NONE, EMIT(normal), AX, WF },
  { 0x07u, "cmpB",   Eb,   Ib,   NONE, EMIT(normal), AX, WF }
};

/* 11 000 011 */
const
OpCode group1_Ev_Iv[8] = {
  { 0x00u, "addL",   Ev,   Iv,   NONE, EMIT(normal), AO, WF },
  { 0x01u, "orL",    Ev,   Iv,   NONE, EMIT(normal), AO, WF },
  { 0x02u, "adcL",   Ev,   Iv,   NONE, EMIT(normal), AO, RWF },
  { 0x03u, "sbbL",   Ev,   Iv,   NONE, EMIT(normal), AO, RWF },
  { 0x04u, "andL",   Ev,   Iv,   NONE, EMIT(normal), AO, WF },
  { 0x05u, "subL",   Ev,   Iv,   NONE, EMIT(normal), AO, WF },
  { 0x06u, "xorL",   Ev,   Iv,   NONE, EMIT(normal), AO, WF },
  { 0x07u, "cmpL",   Ev,   Iv,   NONE, EMIT(normal), AO, WF }
};

const
OpCode group1_Ev_Ib[8] = {
  { 0x00u, "addL",   Ev,   Ib,   NONE, EMIT(normal), AO, WF },
  { 0x01u, "orL",    Ev,   Ib,   NONE, EMIT(normal), AO, WF },
  { 0x02u, "adcL",   Ev,   Ib,   NONE, EMIT(normal), AO, RWF },
  { 0x03u, "sbbL",   Ev,   Ib,   NONE, EMIT(normal), AO, RWF },
  { 0x04u, "andL",   Ev,   Ib,   NONE, EMIT(normal), AO, WF },
  { 0x05u, "subL",   Ev,   Ib,   NONE, EMIT(normal), AO, WF },
  { 0x06u, "xorL",   Ev,   Ib,   NONE, EMIT(normal), AO, WF },
  { 0x07u, "cmpL",   Ev,   Ib,   NONE, EMIT(normal), AO, WF }
};

const
OpCode group2a_Eb_Ib[8] = {
  { 0x00u, "rolB",   Eb,   Ib,   NONE, EMIT(normal), AX, WPF },
  { 0x01u, "rorB",   Eb,   Ib,   NONE, EMIT(normal), AX, WPF },
  { 0x02u, "rclB",   Eb,   Ib,   NONE, EMIT(normal), AX, RWF },
  { 0x03u, "rcrB",   Eb,   Ib,   NONE, EMIT(normal), AX, RWF },
  { 0x04u, "shlB",   Eb,   Ib,   NONE, EMIT(normal), AX, WF },
  { 0x05u, "shrB",   Eb,   Ib,   NONE, EMIT(normal), AX, WF },
  { 0x06u, RESERVED },
  { 0x07u, "sarB",   Eb,   Ib,   NONE, EMIT(normal), AX, WF }
};

const
OpCode group2a_Ev_Ib[8] = {
  { 0x00u, "rolL",   Ev,   Ib,   NONE, EMIT(normal), AO, WPF },
  { 0x01u, "rorL",   Ev,   Ib,   NONE, EMIT(normal), AO, WPF },
  { 0x02u, "rclL",   Ev,   Ib,   NONE, EMIT(normal), AO, RWF },
  { 0x03u, "rcrL",   Ev,   Ib,   NONE, EMIT(normal), AO, RWF },
  { 0x04u, "shlL",   Ev,   Ib,   NONE, EMIT(normal), AO, WF },
  { 0x05u, "shrL",   Ev,   Ib,   NONE, EMIT(normal), AO, WF },
  { 0x06u, RESERVED },
  { 0x07u, "sarL",   Ev,   Ib,   NONE, EMIT(normal), AO, WF }
};

const
OpCode group2_Eb_1[8] = {
  { 0x00u, "rolB",   Eb,   NONE, NONE, EMIT(normal), AX, WPF },
  { 0x01u, "rorB",   Eb,   NONE, NONE, EMIT(normal), AX, WPF },
  { 0x02u, "rclB",   Eb,   NONE, NONE, EMIT(normal), AX, RWF },
  { 0x03u, "rcrB",   Eb,   NONE, NONE, EMIT(normal), AX, RWF },
  { 0x04u, "shlB",   Eb,   NONE, NONE, EMIT(normal), AX, WF },
  { 0x05u, "shrB",   Eb,   NONE, NONE, EMIT(normal), AX, WF },
  { 0x06u, RESERVED },
  { 0x07u, "sarB",   Eb,   NONE,   NONE, EMIT(normal), AX, WF }
};

const
OpCode group2_Ev_1[8] = {
  { 0x00u, "rolL",   Ev,   NONE, NONE, EMIT(normal), AO, WPF },
  { 0x01u, "rorL",   Ev,   NONE, NONE, EMIT(normal), AO, WPF },
  { 0x02u, "rclL",   Ev,   NONE, NONE, EMIT(normal), AO, RWF },
  { 0x03u, "rcrL",   Ev,   NONE, NONE, EMIT(normal), AO, RWF },
  { 0x04u, "shlL",   Ev,   NONE, NONE, EMIT(normal), AO, WF },
  { 0x05u, "shrL",   Ev,   NONE, NONE, EMIT(normal), AO, WF },
  { 0x06u, RESERVED },
  { 0x07u, "sarL",   Ev,   NONE, NONE, EMIT(normal), AO, WF }
};

const
OpCode group2_Eb_CL[8] = {
  { 0x00u, "rolB",   Eb,   CL,   NONE, EMIT(normal), AX, WPF },
  { 0x01u, "rorB",   Eb,   CL,   NONE, EMIT(normal), AX, WPF },
  { 0x02u, "rclB",   Eb,   CL,   NONE, EMIT(normal), AX, RWF },
  { 0x03u, "rcrB",   Eb,   CL,   NONE, EMIT(normal), AX, RWF },
  { 0x04u, "shlB",   Eb,   CL,   NONE, EMIT(normal), AX, WF },
  { 0x05u, "shrB",   Eb,   CL,   NONE, EMIT(normal), AX, WF },
  { 0x06u, RESERVED },
  { 0x07u, "sarB",   Eb,   CL,   NONE, EMIT(normal), AX, WF }
};

const
OpCode group2_Ev_CL[8] = {
  { 0x00u, "rolL",   Ev,   CL,   NONE, EMIT(normal), AO, WPF },
  { 0x01u, "rorL",   Ev,   CL,   NONE, EMIT(normal), AO, WPF },
  { 0x02u, "rclL",   Ev,   CL,   NONE, EMIT(normal), AO, RWF },
  { 0x03u, "rcrL",   Ev,   CL,   NONE, EMIT(normal), AO, RWF },
  { 0x04u, "shlL",   Ev,   CL,   NONE, EMIT(normal), AO, WF },
  { 0x05u, "shrL",   Ev,   CL,   NONE, EMIT(normal), AO, WF },
  { 0x06u, RESERVED },
  { 0x07u, "sarL",   Ev,   CL,   NONE, EMIT(normal), AO, WF }
};

const
OpCode group3b[8] = {
  { 0x00u, "testB",  Eb,   Ib, NONE, EMIT(normal), AX, WF },
  { 0x01u, RESERVED },
  { 0x02u, "notB",   Eb,   NONE, NONE, EMIT(normal), AX, WF },
  { 0x03u, "negB",   Eb,   NONE, NONE, EMIT(normal), AX, WF },
  { 0x04u, "mulB",   AL,   Eb,   NONE, EMIT(normal), AX, WF },
  { 0x05u, "imulB",  AL,   Eb,   NONE, EMIT(normal), AX, WF },
  { 0x06u, "divB",   AL,   Eb,   NONE, EMIT(normal), AX, WF },
  { 0x07u, "idivB",  AL,   Eb,   NONE, EMIT(normal), AX, WF }
};

const
OpCode group3v[8] = {
  { 0x00u, "testL",  Ev,   Iv,   NONE, EMIT(normal), AO, WF },
  { 0x01u, RESERVED },
  { 0x02u, "notL",   Ev,     NONE, NONE, EMIT(normal), AO, WF },
  { 0x03u, "negL",   Ev,     NONE, NONE, EMIT(normal), AO, WF },
  { 0x04u, "mulL",   eAX,    Ev,   NONE, EMIT(normal), AO, WF },
  { 0x05u, "imulL",  eAX,    Ev,   NONE, EMIT(normal), AO, WF },
  { 0x06u, "divL",   eAX,    Ev,   NONE, EMIT(normal), AO, WF },
  { 0x06u, "idivL",  eAX,    Ev,   NONE, EMIT(normal), AO, WF }
};

const
OpCode ngroup4[8] = {
  { 0x00u, "incB",   Eb,     NONE, NONE, EMIT(normal), AX, WAOF },
  { 0x01u, "decB",   Eb,     NONE, NONE, EMIT(normal), AX, WAOF },
  { 0x02u, RESERVED },
  { 0x03u, RESERVED },
  { 0x04u, RESERVED },
  { 0x05u, RESERVED },
  { 0x06u, RESERVED },
  { 0x07u, RESERVED }
};

const
OpCode ngroup5[8] = {
  { 0x00u, "incL",   Ev,     NONE, NONE, EMIT(normal), AO, WAOF },
  { 0x01u, "decL",   Ev,     NONE, NONE, EMIT(normal), AO, WAOF },
  { 0x02u, "callL",  Ev,     NONE, NONE, EMIT(call_near_mem), AO | JJ, NF },
  { 0x03u, "call",   Ep,     NONE, NONE, EMIT(normal), AO, NF },
  { 0x04u, "jmpL",   Ev,     NONE, NONE, EMIT(jmp_near_mem), AO | JJ, NF },
  { 0x05u, "jmp",    Ep,     NONE, NONE, EMIT(normal), AO, NF },
  { 0x06u, "pushL",  Ev,     NONE, NONE, EMIT(normal), AO, NF },
  { 0x07u, RESERVED }
};

const
OpCode ngroup6[8] = {
  { 0x00u, "sldt",   Ew,     NONE, NONE, EMIT(normal), AX, NF },
  { 0x01u, "str",    Ew,     NONE, NONE, EMIT(normal), AX, NF },
  { 0x02u, "lldt",   Ew,     NONE, NONE, EMIT(normal), AX, NF },
  { 0x03u, "ltr",    Ew,     NONE, NONE, EMIT(normal), AX, NF },
  { 0x04u, "verr",   Ew,     NONE, NONE, EMIT(normal), AX, WPF },
  { 0x05u, "verw",   Ew,     NONE, NONE, EMIT(normal), AX, WPF },
  { 0x06u, RESERVED },
  { 0x07u, RESERVED }
};

const
OpCode ngroup7[8] = {
  { 0x00u, "sgdt",   Ms,     NONE, NONE, EMIT(normal),   AO, NF },
  { 0x01u, "sidt",   Ms,     NONE, NONE, EMIT(normal),   AO, NF },
  { 0x02u, "lgdt",   Ms,     NONE, NONE, EMIT(normal),   AO, NF },
  { 0x03u, "lidt",   Ms,     NONE, NONE, EMIT(normal),   AO, NF },
  { 0x04u, "smsw",   Ew,     NONE, NONE, EMIT(normal),   AO, NF },
  { 0x05u, RESERVED },
  { 0x06u, "lmsw",   Ew,     NONE, NONE, EMIT(normal),   AX, NF },
  { 0x07u, "invlpg", Mb,     NONE, NONE, EMIT(normal),   AX, NF }
};

const
OpCode group8_Ev_Ib[8] = {
  { 0x00u, RESERVED },
  { 0x01u, RESERVED },
  { 0x02u, RESERVED },
  { 0x03u, RESERVED },
  { 0x04u, "btL",    Ev,     Ib,   NONE, EMIT(normal), AO, WF },
  { 0x05u, "btsL",   Ev,     Ib,   NONE, EMIT(normal), AO, WF },
  { 0x06u, "btrL",   Ev,     Ib,   NONE, EMIT(normal), AO, WF },
  { 0x07u, "btcL",   Ev,     Ib,   NONE, EMIT(normal), AO, WF }
};

const
OpCode ngroup9[8] = {
  { 0x00u, RESERVED },
  { 0x01u, "cmpxchgL",Ev,  NONE,   NONE, EMIT(normal), AX, WF },
  { 0x02u, RESERVED },
  { 0x03u, RESERVED },
  { 0x04u, RESERVED },
  { 0x05u, RESERVED },
  { 0x06u, RESERVED },
  { 0x07u, RESERVED }
};


const
OpCode float_d8[16] = {
  { 0x00u, "fadd-sr",    Md,   NONE,  NONE, EMIT(normal), AX, NF },
  { 0x01u, "fmul-sr",    Md,   NONE,  NONE, EMIT(normal), AX, NF },
  { 0x02u, "fcom-sr",    Md,   NONE,  NONE, EMIT(normal), AX, NF },
  { 0x03u, "fcomp-sr",   Md,   NONE,  NONE, EMIT(normal), AX, NF },
  { 0x04u, "fsub-sr",    Md,   NONE,  NONE, EMIT(normal), AX, NF },
  { 0x05u, "fsubr-sr",   Md,   NONE,  NONE, EMIT(normal), AX, NF },
  { 0x06u, "fdiv-sr",    Md,   NONE,  NONE, EMIT(normal), AX, NF },
  { 0x07u, "fdivr-sr",   Md,   NONE,  NONE, EMIT(normal), AX, NF },

  { 0x08u, "fadd",    FREG, NONE,  NONE, EMIT(normal), DF_BINARY },
  { 0x09u, "fmul",    FREG, NONE,  NONE, EMIT(normal), DF_BINARY },
  { 0x0au, "fcom",    FREG, NONE,  NONE, EMIT(normal), DF_BINARY },
  { 0x0bu, "fcomp",   FREG, NONE,  NONE, EMIT(normal), DF_BINARY },
  { 0x0cu, "fsub",    FREG, NONE,  NONE, EMIT(normal), DF_BINARY },
  { 0x0du, "fsubr",   FREG, NONE,  NONE, EMIT(normal), DF_BINARY },
  { 0x0eu, "fdiv",    FREG, NONE,  NONE, EMIT(normal), DF_BINARY },
  { 0x0fu, "fdivr",   FREG, NONE,  NONE, EMIT(normal), DF_BINARY }
};


const
OpCode float_d9[16] = {
  { 0x00u, "fld-sr",     Md,   NONE,  NONE, EMIT(normal), AX },
  { 0x01u, RESERVED },
  { 0x02u, "fst-sr",     Md,   NONE,  NONE, EMIT(normal), AX },
  { 0x03u, "fstp-sr",    Md,   NONE,  NONE, EMIT(normal), AX },
  { 0x04u, "fldenv-z",   Mz,   NONE,  NONE, EMIT(normal), AX },
  { 0x05u, "fldcw-w",    Mw,   NONE,  NONE, EMIT(normal), AX },
  { 0x06u, "fstenv-z",   Mz,   NONE,  NONE, EMIT(normal), AX },
  { 0x07u, "fstcw-w",    Mw,   NONE,  NONE, EMIT(normal), AX },

  { 0x08u, "fld",     FREG, NONE,  NONE, EMIT(normal), DF_BINARY },
  { 0x09u, "fxch",    FREG, NONE,  NONE, EMIT(normal), DF_BINARY },
  { 0x0au, FTABLE(float_d9_2) },
  { 0x0bu, RESERVED },
  { 0x0cu, FTABLE(float_d9_4) },
  { 0x0du, FTABLE(float_d9_5) },
  { 0x0eu, FTABLE(float_d9_6) },
  { 0x0fu, FTABLE(float_d9_7) }
};

const
OpCode float_d9_2[8] = {
  { 0x00u, "fnop",    NONE, NONE,  NONE, EMIT(normal) },
  { 0x01u, RESERVED },
  { 0x02u, RESERVED },
  { 0x03u, RESERVED },
  { 0x04u, RESERVED },
  { 0x05u, RESERVED },
  { 0x06u, RESERVED },
  { 0x07u, RESERVED }
};

const
OpCode float_d9_4[8] = {
  { 0x00u, "fchs",    NONE, NONE,  NONE, EMIT(normal) },
  { 0x01u, "fabs",    NONE, NONE,  NONE, EMIT(normal) },
  { 0x02u, RESERVED },
  { 0x03u, RESERVED },
  { 0x04u, "ftst",    NONE, NONE,  NONE, EMIT(normal) },
  { 0x05u, "fxam",    NONE, NONE,  NONE, EMIT(normal) },
  { 0x06u, RESERVED },
  { 0x07u, RESERVED }
};

const
OpCode float_d9_5[8] = {
  { 0x00u, "fld1",    NONE, NONE,  NONE, EMIT(normal) },
  { 0x01u, "fldl2t",  NONE, NONE,  NONE, EMIT(normal) },
  { 0x02u, "fldl2e",  NONE, NONE,  NONE, EMIT(normal) },
  { 0x03u, "fldpi",   NONE, NONE,  NONE, EMIT(normal) },
  { 0x04u, "fldlg2",  NONE, NONE,  NONE, EMIT(normal) },
  { 0x05u, "fldln2",  NONE, NONE,  NONE, EMIT(normal) },
  { 0x06u, "fldz",    NONE, NONE,  NONE, EMIT(normal) },
  { 0x07u, RESERVED }
};

const
OpCode float_d9_6[8] = {
  { 0x00u, "f2xm1",   NONE, NONE,  NONE, EMIT(normal) },
  { 0x01u, "fyl2x",   NONE, NONE,  NONE, EMIT(normal) },
  { 0x02u, "fptan",   NONE, NONE,  NONE, EMIT(normal) },
  { 0x03u, "fpatan",  NONE, NONE,  NONE, EMIT(normal) },
  { 0x04u, "fxtract", NONE, NONE,  NONE, EMIT(normal) },
  { 0x05u, "fprem1",  NONE, NONE,  NONE, EMIT(normal) },
  { 0x06u, "fdecstp", NONE, NONE,  NONE, EMIT(normal) },
  { 0x07u, "fincstp", NONE, NONE,  NONE, EMIT(normal) }
};

const
OpCode float_d9_7[8] = {
  { 0x00u, "fprem",   NONE, NONE,  NONE, EMIT(normal) },
  { 0x01u, "fyl2xp1", NONE, NONE,  NONE, EMIT(normal) },
  { 0x02u, "fsqrt",   NONE, NONE,  NONE, EMIT(normal) },
  { 0x03u, "fsincos", NONE, NONE,  NONE, EMIT(normal) },
  { 0x04u, "frndint", NONE, NONE,  NONE, EMIT(normal) },
  { 0x05u, "fscale",  NONE, NONE,  NONE, EMIT(normal) },
  { 0x06u, "fsin",    NONE, NONE,  NONE, EMIT(normal) },
  { 0x07u, "fcos",    NONE, NONE,  NONE, EMIT(normal) }
};


const
OpCode float_da[16] = {
  { 0x00u, "fiadd-d",   Md,   NONE,  NONE, EMIT(normal), AX },
  { 0x01u, "fimul-d",   Md,   NONE,  NONE, EMIT(normal), AX },
  { 0x02u, "ficom-d",   Md,   NONE,  NONE, EMIT(normal), AX },
  { 0x03u, "ficomp-d",  Md,   NONE,  NONE, EMIT(normal), AX },
  { 0x04u, "fisub-d",   Md,   NONE,  NONE, EMIT(normal), AX },
  { 0x05u, "fisubr-d",  Md,   NONE,  NONE, EMIT(normal), AX },
  { 0x06u, "fidiv-d",   Md,   NONE,  NONE, EMIT(normal), AX },
  { 0x07u, "fidivr-d",  Md,   NONE,  NONE, EMIT(normal), AX },
  
  { 0x08u, "fcmovb",  FREG, NONE,  NONE, EMIT(normal), DF_BINARY, RF },
  { 0x09u, "fcmove",  FREG, NONE,  NONE, EMIT(normal), DF_BINARY, RF },
  { 0x0au, "fcmovbe", FREG, NONE,  NONE, EMIT(normal), DF_BINARY, RF },
  { 0x0bu, "fcmovu",  FREG, NONE,  NONE, EMIT(normal), DF_BINARY, RF },
  { 0x0cu, RESERVED },
  { 0x0du, FTABLE(float_da_5) },
  { 0x0eu, RESERVED },
  { 0x0fu, RESERVED }
};

const
OpCode float_da_5[8] = {
  { 0x00u, RESERVED },
  { 0x01u, "fucompp", NONE, NONE,  NONE, EMIT(normal) },
  { 0x02u, RESERVED },
  { 0x03u, RESERVED },
  { 0x04u, RESERVED },
  { 0x05u, RESERVED },
  { 0x06u, RESERVED },
  { 0x07u, RESERVED }
};


const
OpCode float_db[16] = {
  { 0x00u, "fild-d",     Md,   NONE,  NONE, EMIT(normal), AX },
  { 0x01u, RESERVED },
  { 0x02u, "fist-d",     Md,   NONE,  NONE, EMIT(normal), AX },
  { 0x03u, "fistp-d",    Md,   NONE,  NONE, EMIT(normal), AX },
  { 0x04u, RESERVED },
  { 0x05u, "fld-er",     My,   NONE,  NONE, EMIT(normal), AX },
  { 0x06u, RESERVED },
  { 0x07u, "fstp-er",    My,   NONE,  NONE, EMIT(normal), AX },

  { 0x08u, "fcmovnb", FREG, NONE,  NONE, EMIT(normal), DF_BINARY, RF },
  { 0x09u, "fcmovne", FREG, NONE,  NONE, EMIT(normal), DF_BINARY, RF },
  { 0x0au, "fcmovnbe",FREG, NONE,  NONE, EMIT(normal), DF_BINARY, RF },
  { 0x0bu, "fcmovnu", FREG, NONE,  NONE, EMIT(normal), DF_BINARY, RF },
  { 0x0cu, FTABLE(float_db_4) },
  { 0x0du, "fucomi",  FREG, NONE,  NONE, EMIT(normal), DF_BINARY, WPF },
  { 0x0eu, "fcomi",   FREG, NONE,  NONE, EMIT(normal), DF_BINARY, WPF },
  { 0x0fu, RESERVED }
};

const
OpCode float_db_4[8] = {
  { 0x00u, RESERVED },
  { 0x01u, RESERVED },
  { 0x02u, "fclex",   NONE, NONE,  NONE, EMIT(normal) },
  { 0x03u, "finit",   NONE, NONE,  NONE, EMIT(normal) },
  { 0x04u, RESERVED },
  { 0x05u, RESERVED },
  { 0x06u, RESERVED },
  { 0x07u, RESERVED }
};


const
OpCode float_dc[16] = {
  { 0x00u, "fadd-dr",    Mq,   NONE,  NONE, EMIT(normal), AX },
  { 0x01u, "fmul-dr",    Mq,   NONE,  NONE, EMIT(normal), AX },
  { 0x02u, "fcom-dr",    Mq,   NONE,  NONE, EMIT(normal), AX },
  { 0x03u, "fcomp-dr",   Mq,   NONE,  NONE, EMIT(normal), AX },
  { 0x04u, "fsub-dr",    Mq,   NONE,  NONE, EMIT(normal), AX },
  { 0x05u, "fsubr-dr",   Mq,   NONE,  NONE, EMIT(normal), AX },
  { 0x06u, "fdiv-dr",    Mq,   NONE,  NONE, EMIT(normal), AX },
  { 0x07u, "fdivr-dr",   Mq,   NONE,  NONE, EMIT(normal), AX },

  { 0x08u, "fadd",    FREG, NONE,  NONE, EMIT(normal), DF_BINARY | DF_DIRECTION },
  { 0x09u, "fmul",    FREG, NONE,  NONE, EMIT(normal), DF_BINARY | DF_DIRECTION },
  { 0x0au, RESERVED },
  { 0x0bu, RESERVED },
  { 0x0cu, "fsubr",   FREG, NONE,  NONE, EMIT(normal), DF_BINARY | DF_DIRECTION },
  { 0x0du, "fsub",    FREG, NONE,  NONE, EMIT(normal), DF_BINARY | DF_DIRECTION },
  { 0x0eu, "fdivr",   FREG, NONE,  NONE, EMIT(normal), DF_BINARY | DF_DIRECTION },
  { 0x0fu, "fdiv",    FREG, NONE,  NONE, EMIT(normal), DF_BINARY | DF_DIRECTION }
};


const
OpCode float_dd[16] = {
  { 0x00u, "fld-dr",     Mq,   NONE,  NONE, EMIT(normal), AX },
  { 0x01u, RESERVED },
  { 0x02u, "fst-dr",     Mq,   NONE,  NONE, EMIT(normal), AX },
  { 0x03u, "fstp-dr",    Mq,   NONE,  NONE, EMIT(normal), AX },
  { 0x04u, "frstor-x",   Mx,   NONE,  NONE, EMIT(normal), AX },
  { 0x05u, RESERVED },
  { 0x06u, "fsave-x",    Mx,   NONE,  NONE, EMIT(normal), AX },
  { 0x07u, "fstsw-w",    Mw,   NONE,  NONE, EMIT(normal), AX },

  { 0x08u, "ffree",   FREG, NONE,  NONE, EMIT(normal) },
  { 0x09u, RESERVED },
  { 0x0au, "fst",     FREG, NONE,  NONE, EMIT(normal) },
  { 0x0bu, "fstp",    FREG, NONE,  NONE, EMIT(normal) },
  { 0x0cu, "fucom",   FREG, NONE,  NONE, EMIT(normal), DF_BINARY | DF_DIRECTION },
  { 0x0du, "fucomp",  FREG, NONE,  NONE, EMIT(normal) },
  { 0x0eu, RESERVED },
  { 0x0fu, RESERVED },
};


const
OpCode float_de[16] = {
  { 0x00u, "fiadd-w",   Mw,   NONE,  NONE, EMIT(normal), AX },
  { 0x01u, "fimul-w",   Mw,   NONE,  NONE, EMIT(normal), AX },
  { 0x02u, "ficom-w",   Mw,   NONE,  NONE, EMIT(normal), AX },
  { 0x03u, "ficomp-w",  Mw,   NONE,  NONE, EMIT(normal), AX },
  { 0x04u, "fisub-w",   Mw,   NONE,  NONE, EMIT(normal), AX },
  { 0x05u, "fisubr-w",  Mw,   NONE,  NONE, EMIT(normal), AX },
  { 0x06u, "fidiv-w",   Mw,   NONE,  NONE, EMIT(normal), AX },
  { 0x07u, "fidivr-w",  Mw,   NONE,  NONE, EMIT(normal), AX },

  { 0x08u, "faddp",   FREG, NONE,  NONE, EMIT(normal), DF_BINARY | DF_DIRECTION },
  { 0x09u, "fmulp",   FREG, NONE,  NONE, EMIT(normal), DF_BINARY | DF_DIRECTION },
  { 0x0au, RESERVED },
  { 0x0bu, FTABLE(float_de_3) },
  { 0x0cu, "fsubrp",  FREG, NONE,  NONE, EMIT(normal), DF_BINARY | DF_DIRECTION },
  { 0x0du, "fsubp",   FREG, NONE,  NONE, EMIT(normal), DF_BINARY | DF_DIRECTION },
  { 0x0eu, "fdivrp",  FREG, NONE,  NONE, EMIT(normal), DF_BINARY | DF_DIRECTION },
  { 0x0fu, "fdivp",   FREG, NONE,  NONE, EMIT(normal), DF_BINARY | DF_DIRECTION },
};

const
OpCode float_de_3[8] = {
  { 0x00u, RESERVED },
  { 0x01u, "fcompp",  NONE, NONE,  NONE, EMIT(normal) },
  { 0x02u, RESERVED },
  { 0x03u, RESERVED },
  { 0x04u, RESERVED },
  { 0x05u, RESERVED },
  { 0x06u, RESERVED },
  { 0x07u, RESERVED }
};


const
OpCode float_df[16] = {
  { 0x00u, "fild-w",    Mw,   NONE,  NONE, EMIT(normal), AX },
  { 0x01u, RESERVED },
  { 0x02u, "fist-w",    Mw,   NONE,  NONE, EMIT(normal), AX },
  { 0x03u, "fistp-w",   Mw,   NONE,  NONE, EMIT(normal), AX },
  { 0x04u, "fbld-y",    My,   NONE,  NONE, EMIT(normal), AX },
  { 0x05u, "fild-q",    Mq,   NONE,  NONE, EMIT(normal), AX },
  { 0x06u, "fbstp-y",   My,   NONE,  NONE, EMIT(normal), AX },
  { 0x07u, "fistp-q",   Mq,   NONE,  NONE, EMIT(normal), AX },

  { 0x08u, RESERVED },
  { 0x09u, RESERVED },
  { 0x0au, RESERVED },
  { 0x0bu, RESERVED },
  { 0x0cu, FTABLE(float_df_4) },
  { 0x0du, "fucomip", FREG, NONE,  NONE, EMIT(normal), DF_BINARY, WPF },
  { 0x0eu, "fcomip", FREG, NONE,  NONE, EMIT(normal), DF_BINARY, WPF },
  { 0x0fu, RESERVED }
};

const
OpCode float_df_4[8] = {
  { 0x00u, "fstsw-ax",   NONE, NONE,  NONE, EMIT(normal) },
  { 0x01u, RESERVED },
  { 0x02u, RESERVED },
  { 0x03u, RESERVED },
  { 0x04u, RESERVED },
  { 0x05u, RESERVED },
  { 0x06u, RESERVED },
  { 0x07u, RESERVED },
};

