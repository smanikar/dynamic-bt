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


/***********************************************************************
                      Some Helper Routines 
************************************************************************/

inline static bb_entry *
make_bb_entry(machine_t *M, unsigned long src, unsigned long dest, unsigned long p) 						
{						
  bb_entry *new_entry;
  bb_entry **lookup_table_entry;	

  if(M->no_of_bbs >= MAX_BBS)	
    panic("MAX_BBS exceededn");

  new_entry = &((M->bb_entry_nodes)[(M->no_of_bbs)++]);
  new_entry->src_bb_eip = src;
  new_entry->trans_bb_eip = dest;
  new_entry->sieve_header = NULL;
  new_entry->proc_entry = p;

#ifdef DIFF_HASH
  lookup_table_entry = &M->lookup_table[((src) - M->guest_start_eip) & (LOOKUP_TABLE_SIZE - 1)];
#else
  lookup_table_entry = &M->lookup_table[(src) & (LOOKUP_TABLE_SIZE - 1)];	
#endif

  new_entry->next = *lookup_table_entry;
  *lookup_table_entry = new_entry;
  
#ifdef PROFILE_BB_STATS  
  new_entry->trace_next = NULL;
  new_entry->nInstr = 0;
  new_entry->flags = 0;
#endif

#ifdef PROFILE_RET_MISS  
  if(p == (unsigned long)COLD_PROC_ENTRY)
    M->ptState->cold_cnt++;
#endif

  return new_entry;
}

static inline void
note_patch(machine_t *M, unsigned char *at, unsigned char *to, unsigned long proc_addr)
{
  M->patch_array[M->patch_count].at = at;
  M->patch_array[M->patch_count].to = to;
  M->patch_array[M->patch_count].proc_addr = proc_addr;
  M->patch_count ++;
}  

static inline bool
continue_trace(machine_t *M, decode_t *d, unsigned long jmp_destn)
{
  /* Find out if we have already translated the target, if so, emit a
     jump to the target bb */
  bb_entry *entry = (bb_entry *)lookup_bb_eip(M, (unsigned long)jmp_destn);
  if(entry == NULL) {
 
    bb_entry *new_bb_entry = make_bb_entry(M, jmp_destn, (unsigned long)M->bbOut, M->curr_bb_entry->proc_entry);
    M->next_eip = (unsigned long) jmp_destn;
    M->curr_bb_entry = new_bb_entry;

    return false;
  }

  if(entry->trans_bb_eip == NOT_YET_TRANSLATED) {

    entry->trans_bb_eip = (unsigned long) M->bbOut;

    M->next_eip = (unsigned long) jmp_destn;
    M->curr_bb_entry = entry;

    return false;
  }

  /* Now, AND jmp_destn with 0x0000FFFFu if 16-bit mode instruction */
  if (!THIRTY_TWO_BIT_INSTR(d))					
    jmp_destn = jmp_destn & 0x0000FFFFu;

  bb_emit_jump (M, 0);		/* Dummy jump instruction which would be patched later by the translator */
  note_patch(M, M->bbOut - 4, (unsigned char *)jmp_destn, M->curr_bb_entry->proc_entry);
  
  return true;
}

