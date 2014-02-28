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

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "switches.h"
#include "debug.h"
#include "machine.h"
#include "decode.h"

static void
show_sib_arg(decode_t *ds, unsigned a, FILE *F)
{
  OpCode *opcode = (OpCode *)ds->pEntry;
  OpArgument *arg = &opcode->args[a];

  char *base = "";
  char *index = "";
  char *scale = "";
  /* Choose the base register first: */
  switch(ds->sib.parts.base) {
  case 0u:
    base = "%eax";
    break;
  case 1u:
    base = "%ecx";
    break;
  case 2u:
    base = "%edx";
    break;
  case 3u:
    base = "%ebx";
    break;
  case 4u:
    base = "%esp";
    break;
  case 5u:
    if (ds->modrm.parts.mod != 0)
      base = "%ebp";
    break;
  case 6u:
    base = "%esi";
    break;
  case 7u:
    base = "%edi";
    break;
  }

  switch(ds->sib.parts.index) {
  case 0u:
    index = "%eax";
    break;
  case 1u:
    index = "%ecx";
    break;
  case 2u:
    index = "%edx";
    break;
  case 3u:
    index = "%ebx";
    break;
  case 4u:
    /* none */
    break;
  case 5u:
    index = "%ebp";
    break;
  case 6u:
    index = "%esi";
    break;
  case 7u:
    index = "%edi";
    break;
  }

  switch(ds->sib.parts.ss) {
  case 0u:
    scale = "1";
    break;
  case 1u:
    scale = "2";
    break;
  case 2u:
    scale = "4";
    break;
  case 3u:
    scale = "8";
    break;
  }

  if (ds->sib.parts.base == 5u &&
      ds->modrm.parts.mod == 0)
    fprintf(F, "0x%x", ds->displacement);
  fprintf(F, "(%s,%s,%s)", base, index, scale);
}

#define VREG(s1) ((ds->opstate & OPSTATE_DATA32) ? "%%e" s1 : "%%" s1)
#define DREG(s1) "%%e" s1
#define BREG(s1) "%%" s1
#define SREG(s1) "%%" s1

static void
show_modrm_arg(decode_t *ds, unsigned a, FILE *F)
{
  OpCode *opcode = (OpCode *)ds->pEntry;
  OpArgument *arg = &opcode->args[a];

  if (ds->opstate & OPSTATE_ADDR32)
    {
      switch(ds->modrm.parts.mod) {
      case 0u:
	{
	  switch(ds->modrm.parts.rm) {
	  case 0u:
	    fprintf(F, "(%%eax)");
	    break;
	  case 1u:
	    fprintf(F, "(%%ecx)");
	    break;
	  case 2u:
	    fprintf(F, "(%%edx)");
	    break;
	  case 3u:
	    fprintf(F, "(%%ebx)");
	    break;
	  case 4u:
	    show_sib_arg(ds, a, F);
	    break;
	  case 5u:
	    fprintf(F, "0x%08x", ds->displacement);
	    break;
	  case 6u:
	    fprintf(F, "(%%esi)");
	    break;
	  case 7u:
	    fprintf(F, "(%%edi)");
	    break;
	  }
	  break;
	}
      case 1u:
      case 2u:
	{
	  fprintf(F, "0x%x", ds->displacement);
	  switch(ds->modrm.parts.rm) {
	  case 0u:
	    fprintf(F, "(%%eax)");
	    break;
	  case 1u:
	    fprintf(F, "(%%ecx)");
	    break;
	  case 2u:
	    fprintf(F, "(%%edx)");
	    break;
	  case 3u:
	    fprintf(F, "(%%ebx)");
	    break;
	  case 4u:
	    show_sib_arg(ds, a, F);
	    break;
	  case 5u:
	    fprintf(F, "(%%ebp)");
	    break;
	  case 6u:
	    fprintf(F, "(%%esi)");
	    break;
	  case 7u:
	    fprintf(F, "(%%edi)");
	    break;
	  }
	  break;
	}
      case 3u:
	{
	  if (arg->ainfo == b_mode) {
	    switch(ds->modrm.parts.rm) {
	    case 0u:
	      fprintf(F, BREG("al"));
	      break;
	    case 1u:
	      fprintf(F, BREG("cl"));
	      break;
	    case 2u:
	      fprintf(F, BREG("dl"));
	      break;
	    case 3u:
	      fprintf(F, BREG("bl"));
	      break;
	    case 4u:
	      fprintf(F, BREG("ah"));
	      break;
	    case 5u:
	      fprintf(F, BREG("ch"));
	      break;
	    case 6u:
	      fprintf(F, BREG("dh"));
	      break;
	    case 7u:
	      fprintf(F, BREG("bh"));
	      break;
	    }
	  }
	  else {
	    switch(ds->modrm.parts.rm) {
	    case 0u:
	      fprintf(F, VREG("ax"));
	      break;
	    case 1u:
	      fprintf(F, VREG("cx"));
	      break;
	    case 2u:
	      fprintf(F, VREG("dx"));
	      break;
	    case 3u:
	      fprintf(F, VREG("bx"));
	      break;
	    case 4u:
	      fprintf(F, VREG("sp"));
	      break;
	    case 5u:
	      fprintf(F, VREG("bp"));
	      break;
	    case 6u:
	      fprintf(F, VREG("si"));
	      break;
	    case 7u:
	      fprintf(F, VREG("di"));
	      break;
	    }
	  }
	  break;
	}
      }
    }
  else
    { 
      switch(ds->modrm.parts.mod) {
      case 0u:
	{
	  switch(ds->modrm.parts.rm) {
	  case 0u:
	    fprintf(F, "(%%bx + %%si)");
	    break;
	  case 1u:
	    fprintf(F, "(%%bx + %%di)");
	    break;
	  case 2u:
	    fprintf(F, "(%%bp + %%si)");
	    break;
	  case 3u:
	    fprintf(F, "(%%bp + %%di)");
	    break;
	  case 4u:
	    fprintf(F, "(%%si)");
	    break;
	  case 5u:
	    fprintf(F, "(%%di)");
	    break;
	  case 6u:
	    fprintf(F, "0x%x", ds->displacement);
	    break;
	  case 7u:
	    fprintf(F, "(%%bx)");
	    break;
	  }
	  break;
	}
      case 1u:
      case 2u:
	{
	  fprintf(F, "0x%x", ds->displacement);
	  switch(ds->modrm.parts.rm) {
	  case 0u:
	    fprintf(F, "(%%bx + %%si)");
	    break;
	  case 1u:
	    fprintf(F, "(%%bx + %%di)");
	    break;
	  case 2u:
	    fprintf(F, "(%%bp + %%si)");
	    break;
	  case 3u:
	    fprintf(F, "(%%bp + %%di)");
	    break;
	  case 4u:
	    fprintf(F, "(%%si)");
	    break;
	  case 5u:
	    fprintf(F, "(%%di)");
	    break;
	  case 6u:
	    fprintf(F, "0x%x", ds->displacement);
	    break;
	  case 7u:
	    fprintf(F, "(%%bx)");
	    break;
	  }
	  break;
	}
      case 3u:
	{
	  if (arg->ainfo == b_mode) {
	    switch(ds->modrm.parts.rm) {
	    case 0u:
	      fprintf(F, BREG("al"));
	      break;
	    case 1u:
	      fprintf(F, BREG("cl"));
	      break;
	    case 2u:
	      fprintf(F, BREG("dl"));
	      break;
	    case 3u:
	      fprintf(F, BREG("bl"));
	      break;
	    case 4u:
	      fprintf(F, BREG("ah"));
	      break;
	    case 5u:
	      fprintf(F, BREG("ch"));
	      break;
	    case 6u:
	      fprintf(F, BREG("dh"));
	      break;
	    case 7u:
	      fprintf(F, BREG("bh"));
	      break;
	    }
	  }
	  else {
	    switch(ds->modrm.parts.rm) {
	    case 0u:
	      fprintf(F, VREG("ax"));
	      break;
	    case 1u:
	      fprintf(F, VREG("cx"));
	      break;
	    case 2u:
	      fprintf(F, VREG("dx"));
	      break;
	    case 3u:
	      fprintf(F, VREG("bx"));
	      break;
	    case 4u:
	      fprintf(F, VREG("sp"));
	      break;
	    case 5u:
	      fprintf(F, VREG("bp"));
	      break;
	    case 6u:
	      fprintf(F, VREG("si"));
	      break;
	    case 7u:
	      fprintf(F, VREG("di"));
	      break;
	    }
	  }
	  break;
	}
      }
    }
}

static void
show_asm_arg(decode_t *ds, unsigned a, FILE *F) 
{
  OpCode *opcode = (OpCode *)ds->pEntry;
  OpArgument *arg = &opcode->args[a];

  switch(arg->amode) {
  case ADDR_implied_reg:
    {
      switch(arg->ainfo) {
      case reg_AH:
	fprintf(F, "%%ah");
	break;
      case reg_AL:
	fprintf(F, "%%al");
	break;
      case reg_BH:
	fprintf(F, "%%bh");
	break;
      case reg_BL:
	fprintf(F, "%%bh");
	break;
      case reg_CH:
	fprintf(F, "%%ch");
	break;
      case reg_CL:
	fprintf(F, "%%ch");
	break;
      case reg_DH:
	fprintf(F, "%%dh");
	break;
      case reg_DL:
	fprintf(F, "%%dh");
	break;
      case reg_DX:
	fprintf(F, "%%dx");
	break;
      case reg_indirDX:
	fprintf(F, "*%%dx");
	break;
      case reg_EAX:
	fprintf(F, VREG("ax"));
	break;
      case reg_EBX:
	fprintf(F, VREG("bx"));
	break;
      case reg_ECX:
	fprintf(F, VREG("cx"));
	break;
      case reg_EDX:
	fprintf(F, VREG("dx"));
	break;
      case reg_ESP:
	fprintf(F, VREG("sp"));
	break;
      case reg_EBP:
	fprintf(F, VREG("bp"));
	break;
      case reg_EDI:
	fprintf(F, VREG("di"));
	break;
      case reg_ESI:
	fprintf(F, VREG("si"));
	break;
      };
      break;
    }

  case ADDR_direct:
    fprintf(F, "$0x%x:$0x%x", ds->imm16,ds->immediate);
    break;
	
  case ADDR_imm:
    {
      if (arg->ainfo == w_mode)
	fprintf(F, "$0x%x", ds->imm16);
      else
	fprintf(F, "$0x%x", ds->immediate);
      break;
    }
  case ADDR_offset:
    {
      fprintf(F, "0x%x", ds->immediate);
      break;
    }
  case ADDR_jmp:
    {
      unsigned len = ds->no_of_prefixes + (ds->pInstr - ds->instr);
      unsigned eip = ds->decode_eip + len;
      eip += ds->immediate;
      fprintf(F, "%x", eip);
    }
    break;
#if 0
  case ADDR_indirE:
    fprintf(F, "*");
    /* fall through */
#endif

  case ADDR_R:
  case ADDR_E:
    show_modrm_arg(ds, a, F);
    break;

  case ADDR_G:
    {
      switch(arg->ainfo) {
      case v_mode:
	{
	  switch (ds->modrm.parts.reg) {
	  case 0u:
	    fprintf(F, VREG("ax"));
	    break;
	  case 1u:
	    fprintf(F, VREG("cx"));
	    break;
	  case 2u:
	    fprintf(F, VREG("dx"));
	    break;
	  case 3u:
	    fprintf(F, VREG("bx"));
	    break;
	  case 4u:
	    fprintf(F, VREG("sp"));
	    break;
	  case 5u:
	    fprintf(F, VREG("bp"));
	    break;
	  case 6u:
	    fprintf(F, VREG("si"));
	    break;
	  case 7u:
	    fprintf(F, VREG("di"));
	    break;
	  }
	  break;
	}
      case b_mode:
	{
	  switch (ds->modrm.parts.reg) {
	  case 0u:
	    fprintf(F, BREG("al"));
	    break;
	  case 1u:
	    fprintf(F, BREG("cl"));
	    break;
	  case 2u:
	    fprintf(F, BREG("dl"));
	    break;
	  case 3u:
	    fprintf(F, BREG("bl"));
	    break;
	  case 4u:
	    fprintf(F, BREG("ah"));
	    break;
	  case 5u:
	    fprintf(F, BREG("ch"));
	    break;
	  case 6u:
	    fprintf(F, BREG("dh"));
	    break;
	  case 7u:
	    fprintf(F, BREG("bh"));
	    break;
	  }
	  break;
	}
      case d_mode:
	{
	  switch (ds->modrm.parts.reg) {
	  case 0u:
	    fprintf(F, DREG("ax"));
	    break;
	  case 1u:
	    fprintf(F, DREG("cx"));
	    break;
	  case 2u:
	    fprintf(F, DREG("dx"));
	    break;
	  case 3u:
	    fprintf(F, DREG("bx"));
	    break;
	  case 4u:
	    fprintf(F, DREG("sp"));
	    break;
	  case 5u:
	    fprintf(F, DREG("bp"));
	    break;
	  case 6u:
	    fprintf(F, DREG("si"));
	    break;
	  case 7u:
	    fprintf(F, DREG("di"));
	    break;
	  }
	  break;
	}
      default:
	fprintf(F, "<unhandled mode %d>", arg->amode);
	break;
      }

      break;
    }

  case ADDR_seg:
    {
      switch(arg->ainfo) {
      case 0u:
	fprintf(F, SREG("es"));
	break;
      case 1u:
	fprintf(F, SREG("cs"));
	break;
      case 2u:
	fprintf(F, SREG("ss"));
	break;
      case 3u:
	fprintf(F, SREG("ds"));
	break;
      case 4u:
	fprintf(F, SREG("fs"));
	break;
      case 5u:
	fprintf(F, SREG("gs"));
	break;
      case 6u:
	fprintf(F, SREG("?seg?"));
	break;
      case 7u:
	fprintf(F, SREG("?seg?"));
	break;
      }
      break;
    }

  case ADDR_seg_reg:
    {
      switch (ds->modrm.parts.reg) {
      case 0u:
	fprintf(F, SREG("es"));
	break;
      case 1u:
	fprintf(F, SREG("cs"));
	break;
      case 2u:
	fprintf(F, SREG("ss"));
	break;
      case 3u:
	fprintf(F, SREG("ds"));
	break;
      case 4u:
	fprintf(F, SREG("fs"));
	break;
      case 5u:
	fprintf(F, SREG("gs"));
	break;
      case 6u:
	fprintf(F, SREG("?seg?"));
	break;
      case 7u:
	fprintf(F, SREG("?seg?"));
	break;
      }
      break;
    }

  case ADDR_ds:
    {
      fprintf(F, "%%ds:(");
      fprintf(F, VREG("si"));
      fprintf(F, ")");
      break;
    }
  case ADDR_es:
    {
      fprintf(F, "%%es:(");
      fprintf(F, VREG("di"));
      fprintf(F, ")");
      break;
    }
  case ADDR_C:
    fprintf(F, "%%cr%d", ds->modrm.parts.reg);
    break;
  case ADDR_D:
    fprintf(F, "%%dbg%d", ds->modrm.parts.reg);
    break;
  case ADDR_FREG:
    if (!(ds->attr & DF_BINARY))
      fprintf(F, "%%ST(%d)", ds->modrm.parts.rm);
    else if (ds->attr & DF_DIRECTION)
      fprintf(F, "%%ST(%d), %%ST(0)", ds->modrm.parts.rm);
    else
      fprintf(F, "%%ST(0), %%ST(%d)", ds->modrm.parts.rm);
    break;
  default:
    fprintf(F, "???AMODE???");
  }
}

void
do_disasm(decode_t *ds, FILE *F)
{
  int a;
  OpCode *opcode = (OpCode *)ds->pEntry;

  unsigned long eip = ds->decode_eip;
  unsigned char *instr = ds->instr;
  unsigned instrLen = ds->pInstr - ds->instr;
  unsigned outputLen = 0;
  char hexbyte[4];
  char ophex[60];
  char opname[40];
  char *pOpname;
  int endByteCount = 0;

  ophex[0] = 0;
  opname[0] = 0;

  fprintf(F, "0x%08x:\t", eip);

  /* First print out the hex encoding */
  if (ds->flags & DSFL_GROUP1_PREFIX) {
    sprintf(hexbyte, "%02x ", ds->Group1_Prefix);
    strcat(ophex, hexbyte);
    outputLen++;
  }
  if (ds->flags & DSFL_GROUP2_PREFIX) {
    sprintf(hexbyte, "%02x ", ds->Group2_Prefix);
    strcat(ophex, hexbyte);
    outputLen++;
  }
  if (ds->flags & DSFL_GROUP3_PREFIX) {
    sprintf(hexbyte, "%02x ", ds->Group3_Prefix);
    strcat(ophex, hexbyte);
    outputLen++;
  }
  if (ds->flags & DSFL_GROUP4_PREFIX) {
    sprintf(hexbyte, "%02x ", ds->Group4_Prefix);
    strcat(ophex, hexbyte);
    outputLen++;
  }

  while (outputLen < 7 && instr != ds->pInstr) {
    sprintf(hexbyte, "%02x ", *instr);
    strcat(ophex, hexbyte);
    outputLen++;
    instr++;
  }

  if (ds->flags & DSFL_GROUP1_PREFIX)
    {
      switch(ds->Group1_Prefix) {
      case PREFIX_LOCK:
	strcat(opname, "lock ");
	break;
      case PREFIX_REPZ:
	strcat(opname, "repz ");
	break;
      case PREFIX_REPNZ:
	strcat(opname, "repnz ");
	break;
      }
    }

  if (ds->flags & DSFL_GROUP2_PREFIX)
    {
      switch(ds->Group2_Prefix) {
      case PREFIX_CS:
	strcat(opname, "cs ");
	break;
      case PREFIX_DS:
	strcat(opname, "ds ");
	break;
      case PREFIX_ES:
	strcat(opname, "es ");
	break;
      case PREFIX_FS:
	strcat(opname, "fs ");
	break;
      case PREFIX_GS:
	strcat(opname, "gs ");
	break;
      case PREFIX_SS:
	strcat(opname, "ss ");
	break;
      }
    }

#if 0
  /* No need to output this, as it is implied in the opcode mnemonic */
  if (ds->flags & DSFL_GROUP3_PREFIX)
    strcat(opname, "data16 ");
#endif

  if (ds->flags & DSFL_GROUP4_PREFIX)
    {
      strcat(opname, "addr16 ");
    }

  pOpname = opname + strlen(opname);

  {
    const char *disasm;

    for (disasm = opcode->disasm; *disasm; disasm++) {
      if (isupper(*disasm)) {
	switch (*disasm) {
	  /*
	   * B -- trailing 'b' suffix
	   * L -- trailing 'w' or 'l' suffix according to mode. 
	   * Q -- trailinq 'd' or 'q' suffix according to mode
	   * W -- b or w (cbw/cwd) according to mode
	   * R -- w or d (cbw/cwd) according to mode
	   * N -- 16 or 32 according to mode (inverted -- for size prefixes)
	   */
	case 'B':
	  {
	    *pOpname++ = 'b';
	    break;
	  }
	case 'L':
	  {
	    if (ds->opstate & OPSTATE_DATA32)
	      *pOpname++ = 'l';
	    else
	      *pOpname++ = 'w';
	    break;
	  }
	case 'Q':
	  {
	    if (ds->opstate & OPSTATE_DATA32)
	      *pOpname++ = 'd';
	    else
	      *pOpname++ = 'q';
	    break;
	  }
	case 'W':
	  {
	    if (ds->opstate & OPSTATE_DATA32)
	      *pOpname++ = 'b';
	    else
	      *pOpname++ = 'w';
	    break;
	  }
	case 'R':
	  {
	    if (ds->opstate & OPSTATE_DATA32)
	      *pOpname++ = 'w';
	    else
	      *pOpname++ = 'd';
	    break;
	  }
	default:
	  {
	    *pOpname++ = *disasm;
	    break;
	  }
	}
      }
      else
	*pOpname++ = *disasm;
    }
    *pOpname= 0;
  }

  fprintf(F, "%-22s", ophex);
  fprintf(F, "%-8s ", opname);

  /* Arg display is pretty silly, because the display order does not
     match the order specified in the decode table. */

  if (opcode->args[2].amode != ADDR_none) {
    show_asm_arg(ds, 2, F);
    fprintf(F, ",");
  }

  if (opcode->args[1].amode != ADDR_none) {
    show_asm_arg(ds, 1, F);
    fprintf(F, ",");
  }
  
  if (opcode->args[0].amode != ADDR_none) {
    show_asm_arg(ds, 0, F);
  }
  
  fprintf(F, "\n");


  /* See if there are more hex bytes to print: */
  endByteCount = 0;
  while (instr != ds->pInstr) {
    outputLen = 0;
    eip += 7;
    ophex[0] = 0;

    while (outputLen < 7 && instr != ds->pInstr) {
      sprintf(hexbyte, "%02x ", *instr);
      strcat(ophex, hexbyte);
      outputLen++;
      instr++;
    }

    fprintf(F, "0x%08x:\t", eip);
    fprintf(F, "%-22s\n", ophex);

    if(endByteCount > 40) {
      fprintf(F, "....\?\?\?\n");
      break;
    }
  }
}
