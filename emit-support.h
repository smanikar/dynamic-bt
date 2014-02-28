#ifndef EMIT_SUPPORT_H
#define EMIT_SUPPORT_H
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

#ifndef INLINE_EMITTERS
/********************************************************************/
/* 			Support Functions			    */				
/********************************************************************/

extern void bb_emit_byte(machine_t *M, unsigned char c);
extern void bb_emit_w16(machine_t *M, unsigned long ul);
extern void bb_emit_w32(machine_t *M, unsigned long ul);
extern void bb_emit_jump(machine_t *M, unsigned char *dest);
extern void bb_emit_ijump(machine_t *M, unsigned long ul);

extern void bb_emit_call(machine_t *M, unsigned char *dest);

extern void bb_emit_indirect_through_reg(machine_t *M, unsigned long reg);

extern void bb_emit_save_reg_to(machine_t *M, unsigned long whichReg, 
				unsigned long addr);
extern void bb_emit_restore_reg_from(machine_t *M, unsigned long whichReg,
				     unsigned long addr);
extern void bb_emit_save_reg(machine_t *M, unsigned long whichReg);
extern void bb_emit_restore_reg(machine_t *M, unsigned long whichReg);
extern void bb_emit_store_immediate_to(machine_t *M, unsigned long imm, unsigned long addr);

void bb_emit_get_4_bytes_into_M(machine_t *M, decode_t * d, unsigned long addr);
void bb_emit_get_2_bytes_into_M(machine_t *M, decode_t * d, unsigned long addr);
#endif

#endif /* EMIT_SUPPORT_H */
