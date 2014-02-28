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
 *
 * Currently handles only the 32-bit instruction set.
 */

#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include "switches.h"
#include "debug.h"
#include "machine.h"
#include "decode.h"
#include "emit.h"

#define READ_IN_NEXT_BYTE() 		\
      do {ds->b = istream_peekByte(M);	\
      istream_nextByte(M); } while(0)				

#define READ_IN_NEXT_WORD() 		\
      do { ds->b = istream_peekWord(M);	\
      istream_nextWord(M); } while(0)

#define READ_IN_NEXT_LONG() 		\
      do { ds->b = istream_peekLong(M);	\
      istream_nextLong(M); } while(0)

static _count = 0;

bool
do_decode(machine_t *M, decode_t *ds)
{
  unsigned i;
  CONST_TABLE OpCode *table = nopbyte0;
  CONST_TABLE OpCode *pEntry = 0;
  int wierd = 0;

  //printf("TEST: do_decode(%x) is called %d times!\n", ds, _count++);

  ds->decode_eip = M->next_eip;
#ifdef STATIC_PASS
  ds->mem_decode_eip = M->mem_next_eip;
#endif
  ds->attr = 0;
  ds->emitfn = 0;
  ds->modrm_regs = 0;	   /* registers sourced by this instruction */
  ds->need_sib = 0;
  ds->dispBytes = 0;
  ds->no_of_prefixes = 0;
  ds->Group1_Prefix=0;
  ds->Group2_Prefix=0;
  ds->Group3_Prefix=0;
  ds->Group4_Prefix=0;
  ds->flags = 0;
  ds->opstate = OPSTATE_DATA32 | OPSTATE_ADDR32;

  /* First, pick off the opcode prefixes. There can be more than one.

  QUESTION: How does the processor behave if the various prefix
  constraints are violated?

  ANSWER: We don't care. We will copy the bytes and the processor
  will behave however the processor behaves */
  
  for(;;){
    ds->b = istream_peekByte(M);
    if (IS_IFAULT(ds->b))
      goto handle_ifault;

    ds->attr = table[ds->b].attr;

    if ((ds->attr & DF_PREFIX) == 0)
      break;
    
    ds->no_of_prefixes ++;

    switch(ds->b) {
    case PREFIX_LOCK:
    case PREFIX_REPZ:
    case PREFIX_REPNZ:
      ds->flags |= DSFL_GROUP1_PREFIX;
      ds->Group1_Prefix = ds->b;
      break;

    case PREFIX_CS:
    case PREFIX_DS:
    case PREFIX_ES:
    case PREFIX_FS:
    case PREFIX_GS:
    case PREFIX_SS:
      ds->flags |= DSFL_GROUP2_PREFIX;
      ds->Group2_Prefix = ds->b;
      break;

    case PREFIX_OPSZ:
      ds->flags |= DSFL_GROUP3_PREFIX;
      ds->opstate ^= OPSTATE_DATA32;
      ds->Group3_Prefix = ds->b;
      break;

    case PREFIX_ADDRSZ:
      ds->flags |= DSFL_GROUP4_PREFIX;
      ds->opstate ^= OPSTATE_ADDR32;
      ds->Group4_Prefix = ds->b;
      break;
    }
    
    /* Note that the prefix bytes are not copied onto the instruction stream; Instead, they are just 
       copied onto their respective placeholders within the decode structure */
    istream_nextByte(M);
  }

  /* Pick off the instruction bytes */
#ifdef STATIC_PASS
  ds->instr = (unsigned char *)M->mem_next_eip;
#else
  ds->instr = (unsigned char *)M->next_eip;
#endif

  for(;;) {
    ds->b = istream_peekByte(M);
    if (IS_IFAULT(ds->b))
      goto handle_ifault;
    pEntry = &table[ds->b];
    ds->attr = pEntry->attr;
    istream_nextByte(M);

    if (ds->attr & DF_TABLE) {
      table = pEntry->ptr;
      //ds->attr &= ~DF_TABLE;
      continue;
    }

    if (ds->attr & DF_FLOAT) {
      wierd = 1;
      table = pEntry->ptr;
      ds->b = istream_peekByte(M);
      if (IS_IFAULT(ds->b))
        goto handle_ifault;
      istream_nextByte(M);
      ds->modrm.byte = ds->b;
      if (ds->modrm.parts.mod == 0x3u) {
	pEntry = &table[ds->modrm.parts.reg + 8];
	ds->attr = pEntry->attr;
	if (ds->attr & DF_ONE_MORE_LEVEL) {
	  table = pEntry->ptr;
	  pEntry = &table[ds->modrm.parts.rm];
	  ds->attr = pEntry->attr;
	}
      }
      else {
	pEntry = &table[ds->modrm.parts.reg];
	ds->attr = pEntry->attr;
      }
    }

    break;
  }

/*   if ( ((ds->Group1_Prefix == 0xF2u) ||  */
/* 	(ds->Group1_Prefix == 0xF3u)) &&  */
/*        (ds->instr[0] == 0x0Fu) && (ds->instr[1] == 0x7Eu) ) { */
/*     ds->attr &= (~DF_MODRM); */
/*     fprintf(stderr, "*** THIS CASE ***\n"); */
/*   } */

  /* If needed, fetch the modR/M byte: */
  if (!wierd) {
    /* i.e., If we have not already eaten up the modR/M byte because of Escape opcodes */
    if (ds->attr & (DF_MODRM|DF_GROUP))
      READ_IN_NEXT_BYTE();
  }

  /* If attr now contains DF_GROUP, then the last byte fetched (the
   * current value of b) was the modrm byte and we need to do one more
   * round of table processing to pick off the proper opcode. */

  if (ds->attr & DF_GROUP) {
    ds->modrm.byte = ds->b;
    pEntry = pEntry->ptr;
    pEntry += ds->modrm.parts.reg;
    ds->attr |= pEntry->attr;
  }

  /* Note that if opcode requires modrm processing then ds->b
   * currently holds the modrm byte.
   */
  if (ds->attr & (DF_MODRM))
    ds->modrm.byte = ds->b;

  /* /attr/ now contains accumulated attributes. /pEntry/ points to
   * last located entry, which specifies what we are going to do in
   * the end. Finish copying modrm arguments and immediate values, if
   * any. 
   */
  if (ds->attr & (DF_MODRM|DF_GROUP)) {
    if (ds->opstate & OPSTATE_ADDR32) {
      /* ds->mod of 00b, 01b, 10b  are the register-indirect cases,
	 except that ds->rm == 100b implies a sib byte and (ds->mod,
	 ds->rm) of (00b, 101b) is disp32. */
      if ((ds->modrm.parts.mod != 0x3u) && (ds->modrm.parts.rm == 4u))
	ds->need_sib = 1;	/* scaled index mode */
      
      if (ds->modrm.parts.mod == 0u && ds->modrm.parts.rm == 5u)
	ds->dispBytes = 4;	/* memory absolute */
      else if (ds->modrm.parts.mod == 1u)
	ds->dispBytes = 1;
      else if (ds->modrm.parts.mod == 2u)
	ds->dispBytes = 4;
    }
    else {  
      /* No SIB byte to consider, but pick off 
         (ds->mod, ds->rm) == (00b,110b) since that is disp16 */
      if (ds->modrm.parts.mod == 0u && ds->modrm.parts.rm == 6u)
	ds->dispBytes = 2;	/* memory absolute */
      else if (ds->modrm.parts.mod == 1u)
	ds->dispBytes = 1;
      else if (ds->modrm.parts.mod == 2u)
	ds->dispBytes = 2;
    }

    if (ds->need_sib) {
      READ_IN_NEXT_BYTE();
      ds->sib.byte = ds->b;
      if ((ds->sib.parts.base == GP_REG_EBP) && (ds->modrm.parts.mod == 0u))
	ds->dispBytes = 4;
    }

    if (ds->dispBytes) {
      if(ds->dispBytes > 2){
	READ_IN_NEXT_LONG();
	ds->displacement = ds->b;
      }else if(ds->dispBytes > 1){
	READ_IN_NEXT_WORD();
	ds->displacement = ds->b;
      } else{
	READ_IN_NEXT_BYTE();
	ds->displacement = ds->b;
      }
		
    }
  }

  /* At this point we have everything except the immediates (or
     offsets, depending on the instruction. Of the instructions that
     take such, only one (ENTER) takes more than one. Iw mode is in
     fact used only by ENTER, RET, and LRET. We therefore proceed by
     handling Iw as a special case. */

  if (ds->attr & DF_Iw) {
    READ_IN_NEXT_WORD();
    ds->imm16 = ds->b;
  }

  if ((ds->attr & DF_Ib) || (ds->attr & DF_Jb)) {
    READ_IN_NEXT_BYTE();
    signed char sc = ds->b;	/* for sign extension */
    ds->immediate = sc;
  }

  if (ds->attr & DF_Iv) {
    if (ds->opstate & OPSTATE_DATA32) {
      READ_IN_NEXT_LONG();
      ds->immediate = ds->b;
    }
    else{
      READ_IN_NEXT_WORD();
      ds->immediate = ds->b;
    }
  }

  if ((ds->attr & DF_Ov) || (ds->attr & DF_Ob)) {

    if (ds->opstate & OPSTATE_ADDR32) {
      READ_IN_NEXT_LONG();
      ds->immediate = ds->b;
    }
    else{
      READ_IN_NEXT_WORD();
      ds->immediate = ds->b;
    }
  }

  if (ds->attr & (DF_Jv|DF_Ap)) {
	
    if (ds->opstate & OPSTATE_DATA32) {
      READ_IN_NEXT_LONG();
      ds->immediate = ds->b;
    }
    else{
      READ_IN_NEXT_WORD();
      ds->immediate = ds->b;
    }
  }

  if (ds->attr & DF_Ap) {
    READ_IN_NEXT_WORD();
    ds->imm16 = ds->b;
  }
  
#ifdef STATIC_PASS
      ds->pInstr = (unsigned char *)M->mem_next_eip;
#else
  ds->pInstr = (unsigned char *)M->next_eip;
#endif

  ds->emitfn = pEntry->ptr;
  ds->pEntry = pEntry;
  
  if (ds->attr & DF_UNDEFINED)    {
    DEBUG(decode)
      printf ("\nUndefined opcode: %2X %2X at %08X\n", ds->instr[0], ds->instr[1], ds->decode_eip);
    return false;
  }

  if (pEntry->ptr == 0)
    return false;

  return true;

 handle_ifault:
  /* We took an instruction fetch fault of some form on this byte. */
#ifdef STATIC_PASS
      ds->pInstr = (unsigned char *)M->mem_next_eip;
#else
  ds->pInstr = (unsigned char *)M->next_eip;
#endif

  ds->emitfn = pEntry->ptr;
  ds->pEntry = pEntry;

  return false;
}
