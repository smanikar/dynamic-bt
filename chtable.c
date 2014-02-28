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


INLINE void 
bb_setup_chash_table(machine_t *M)
{
  int i;
  for (i=0 ; i<CNBUCKETS ; i++)  {
    bb_emit_jump(M, M->cslow_dispatch_bb);
    M->bbOut += 3;
  }
}

void 
xlate_for_csieve (machine_t *M)
{
  bb_entry *entry_node;
  bucket_entry *bucket;	/* Hash bucket onto which this basic-block 
			   will chain onto (at the head)   	*/
  unsigned char *next_instr; /* Sequentially next instr of the above - 
				used just to compute relative jump destination	*/
  unsigned long node; /* A node of the hash chain */
   
/*   fprintf(DBG, "csieve Calling xlate_bb at %lx\n", M->fixregs.eip); */
  M->comming_from_call_indirect = true;
  entry_node = xlate_bb(M);

  bucket = (bucket_entry *) CSIEVE_HASH_BUCKET(M->chash_table, ((unsigned long)entry_node->src_bb_eip));
  //  fprintf(DBG, "Hash bucket start at %lx, this = %lx\n", M->hash_table, bucket);

  /*** Sensitive to the size of Jump Instruction in Bucket ***/
  next_instr = ((unsigned char *)bucket) + 5  ;

  node = (unsigned long) (next_instr + bucket->rel);
  bucket->rel = M->bbOut - next_instr;

  /* mov 0x4(%esp),%ecx */
  bb_emit_byte(M, 0x8bu); // 8b /r
  bb_emit_byte(M, 0x4cu); // 01 001 100
  bb_emit_byte(M, 0x24u); // 00 100 100
  bb_emit_byte(M, 0x4u);

  /* lea -entry->src_bb_eip(%ecx),%ecx */
  bb_emit_byte(M, 0x8Du); // 8D /r
  bb_emit_byte(M, 0x89u); // 10 001 001 
  bb_emit_w32(M, (-((long)(entry_node->src_bb_eip))));

  /* jecxz equal */
  bb_emit_byte(M, 0xe3u);
  bb_emit_byte(M, 0x05u);

  /* jmp $next_bucket */
  bb_emit_jump(M, (unsigned char *)node);

  /* equal: pop %ecx */
  bb_emit_byte(M, 0x59u);

  /* leal 4(%esp) %esp */
  bb_emit_byte(M, 0x8du); // 8d /r
  bb_emit_byte(M, 0x64u); // 01 100 100
  bb_emit_byte(M, 0x24u); // 00 100 100
  bb_emit_byte(M, 0x4u);

  /* jmp $translated_block */
  bb_emit_jump (M, (unsigned char *)entry_node->trans_bb_eip);
 
  M->jmp_target = (unsigned char *)entry_node->trans_bb_eip;
#ifdef PROFILE
  M->ptState->hash_nodes_cnt++;
#endif
}

INLINE void 
bb_setup_cslow_dispatch_bb(machine_t *M)
{
  /* Emit the special BB that first translates the destination basic-block 
     Found and then transfers control into the basic block. */

  /*  fast_dispatch_bb who is my caller, would have done a Push %ecx */
  /* Pop %ecx */
  bb_emit_byte(M, 0x59u);

  /* pop M->fixregs.eip */
  bb_emit_byte(M, 0x8Fu); // 8F /0
  bb_emit_byte(M, 0x05u); // 00 000 101
  bb_emit_w32(M, MREG(M, eip));

  bb_emit_byte(M, 0x9cu);		/* PUSHF */
  bb_emit_byte(M, 0x60u);		/* PUSHA */

  bb_emit_byte(M, 0x68u);		/* PUSH imm32:M */
  bb_emit_w32(M, (unsigned long)M);     /* Argument for xlate_sieve ... */

  bb_emit_byte(M, 0xE8u);		/* CALL emit_csieve_header_xlate_bb */
  bb_emit_w32(M, ((unsigned char*)&xlate_for_csieve) - (M->bbOut + 4));

  bb_emit_byte (M, 0x58u); 		/* POP r32: EAX */

  bb_emit_byte(M, 0x61u);		/* POPA */
  bb_emit_byte(M, 0x9du);		/* POPF */

  //bb_emit_byte(M, 0x65u); /* GS Segment Override Prefix - for accessing the M structure */
  bb_emit_byte(M, 0xFFu); /* JMP [jmp_target] */
  bb_emit_byte(M, 0x25u); /* 00 100 101 */
  bb_emit_w32(M, MFLD(M, jmp_target)); /* Jump to the newly translated basic-block */
}

INLINE void
bb_setup_cfast_dispatch_bb(machine_t *M)
{
  /* Emit the special BB that transfers control from
     one basic-block to another within the basic block cache. */

/*   /\* push %ecx *\/ */
/*   bb_emit_byte(M, 0x51u); */

  /* mov 0x4(%esp),%ecx */
  bb_emit_byte(M, 0x8bu); // 8b /r
  bb_emit_byte(M, 0x4cu); // 01 001 100
  bb_emit_byte(M, 0x24u); // 00 100 100
  bb_emit_byte(M, 0x4u);

#ifndef SMALL_HASH  
  /* lea 0x0(,%ecx,2),%ecx */
  bb_emit_byte(M, 0x8Du); // 8D /r
  bb_emit_byte(M, 0x0Cu); // 00 001 100
  bb_emit_byte(M, 0x4du); // 01 001 101
  bb_emit_w32(M, 0x0u);   // This 0 word is needed. There is no other
                          // There is no other addressing mode.

  /* movzwl %cx,%ecx */
  bb_emit_byte(M, 0x0Fu); // 0F B7 /R
  bb_emit_byte(M, 0xB7u);
  bb_emit_byte(M, 0xC9u); // 11 001 001

  /* lea M->chash_table(,%ecx,2),%ecx */
  bb_emit_byte(M, 0x8Du); // 8D /r
  bb_emit_byte(M, 0x0Cu); // 00 001 100
  bb_emit_byte(M, 0x8du); // 10 001 101
  bb_emit_w32(M, (unsigned long)M->chash_table);

#else
  /* lea 0x0(,%ecx,4),%ecx */
  bb_emit_byte(M, 0x8Du); // 8D /r
  bb_emit_byte(M, 0x0Cu); // 00 001 100
  bb_emit_byte(M, 0x8du); // 10 001 101
  bb_emit_w32(M, 0x0u);   // This 0 word is needed. There is no other
                          // There is no other addressing mode.

  /* movzwl %cx,%ecx */
  bb_emit_byte(M, 0x0Fu); // 0F B7 /R
  bb_emit_byte(M, 0xB7u);
  bb_emit_byte(M, 0xC9u); // 11 001 001

  /* lea M->chash_table(,%ecx,2),%ecx */
  bb_emit_byte(M, 0x8Du); // 8D /r
  bb_emit_byte(M, 0x0Cu); // 00 001 100
  bb_emit_byte(M, 0x4du); // 01 001 101
  bb_emit_w32(M, (unsigned long)M->chash_table);
#endif

  //50:	ff e1                	jmp    *%ecx
  bb_emit_byte(M, 0xffu);
  bb_emit_byte(M, 0xe1u);
/*   /\* jmp *M->hash_table(,%ecx,4) *\/ */
/*   bb_emit_byte(M, 0xFFu); // FF /4 */
/*   bb_emit_byte(M, 0x24u); // 00 100 100 */
/*   bb_emit_byte(M, 0x8Du); // 10 001 101 */
/*   bb_emit_w32(M, (unsigned long) M->hash_table); */
}

