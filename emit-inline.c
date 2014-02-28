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

/** BASIC EMITTER FUNCTIONS. THESE ARE ALL THINGS THAT SHOULD BE 
    INLINED IF POSSIBLE. */

INLINE void
bb_emit_byte(machine_t *M, unsigned char c)
{
  /* We don't bother to signal an error -- overrun will be checked by
     the basic block translator procedure. Since the decoder is never
     a consumer of the output bytes, it's okay to just suppress output
     on overrun. Therefore, simply make sure that we do not run off
     the end of the basic block. */

  if (M->bbOut != M->bbLimit)
    *M->bbOut++ = c;
}

INLINE void
bb_emit_w16(machine_t *M, unsigned long ul)
{
  bb_emit_byte(M, ul & 0xffu);
  bb_emit_byte(M, (ul >> 8) & 0xffu);
}

INLINE void
bb_emit_w32(machine_t *M, unsigned long ul)
{
  bb_emit_byte(M, ul & 0xffu);
  bb_emit_byte(M, (ul >> 8) & 0xffu);
  bb_emit_byte(M, (ul >> 16) & 0xffu);
  bb_emit_byte(M, (ul >> 24) & 0xffu);
}

INLINE void
bb_emit_jump(machine_t *M, unsigned char *dest)
{
  unsigned long next_instr;
  unsigned long moffset;
  
  bb_emit_byte(M, 0xe9u);

  /* Specification for jump target is that it is an offset relative to
     the address of the NEXT instruction, so we need to compute what
     the address of the hypothetical next instruction would be, which
     is now bbOut + 4. */
  next_instr = (unsigned long) M->bbOut + 4;
  moffset = (unsigned long)dest - next_instr;
  bb_emit_w32(M, moffset);
}

INLINE void
bb_emit_call(machine_t *M, unsigned char *dest)
{
  unsigned long next_instr;
  unsigned long moffset;
  
  bb_emit_byte(M, 0xe8u);

  /* Specification for jump target is that it is an offset relative to
     the address of the NEXT instruction, so we need to compute what
     the address of the hypothetical next instruction would be, which
     is now bbOut + 4. */
  next_instr = (unsigned long) M->bbOut + 4;
  moffset = (unsigned long)dest - next_instr;
  bb_emit_w32(M, moffset);
}

INLINE void
bb_emit_save_reg_to(machine_t *M, unsigned long whichReg, unsigned long addr)
{
  //bb_emit_byte(M, 0x65u); /* GS Segment Override Prefix - for accessing the M structure */

  switch(whichReg) {
  case GP_REG_EAX:
    bb_emit_byte(M, 0xa3u);
    bb_emit_w32(M, addr);
    break;
  case GP_REG_EBX:
    bb_emit_byte(M, 0x89u);
    bb_emit_byte(M, 0x1du);
    bb_emit_w32(M, addr);
    break;
  case GP_REG_ECX:
    bb_emit_byte(M, 0x89u);
    bb_emit_byte(M, 0x0du);
    bb_emit_w32(M, addr);
    break;
  case GP_REG_EDX:
    bb_emit_byte(M, 0x89u);
    bb_emit_byte(M, 0x15u);
    bb_emit_w32(M, addr);
    break;
  case GP_REG_ESI:
    bb_emit_byte(M, 0x89u);
    bb_emit_byte(M, 0x35u);
    bb_emit_w32(M, addr);
    break;
  case GP_REG_EDI:
    bb_emit_byte(M, 0x89u);
    bb_emit_byte(M, 0x3du);
    bb_emit_w32(M, addr);
    break;
  case GP_REG_EBP:
    bb_emit_byte(M, 0x89u);
    bb_emit_byte(M, 0x2du);
    bb_emit_w32(M, addr);
    break;
  case GP_REG_ESP:
    bb_emit_byte(M, 0x89u);
    bb_emit_byte(M, 0x25u);
    bb_emit_w32(M, addr);
    break;
  default:
    panic("bb_emit_save_reg_to() called with unknown register\n");
    break;
  }
}

INLINE void
bb_emit_16_bit_save_reg_to(machine_t *M, unsigned long whichReg, unsigned long addr)
{
  //bb_emit_byte(M, 0x65u); /* GS Segment Override Prefix - for accessing the M structure */

  switch(whichReg) {
  case GP_REG_EAX:
    bb_emit_byte(M, 0x66u); /* Operand-size Override Prefix - to indicate 16-bit mov */
    bb_emit_byte(M, 0xa3u);
    bb_emit_w32(M, addr);
    break;
  case GP_REG_EBX:
    bb_emit_byte(M, 0x66u); /* Operand-size Override Prefix - to indicate 16-bit mov */
    bb_emit_byte(M, 0x89u);
    bb_emit_byte(M, 0x1du);
    bb_emit_w32(M, addr);
    break;
  case GP_REG_ECX:
    bb_emit_byte(M, 0x66u); /* Operand-size Override Prefix - to indicate 16-bit mov */
    bb_emit_byte(M, 0x89u);
    bb_emit_byte(M, 0x0du);
    bb_emit_w32(M, addr);
    break;
  case GP_REG_EDX:
    bb_emit_byte(M, 0x66u); /* Operand-size Override Prefix - to indicate 16-bit mov */
    bb_emit_byte(M, 0x89u);
    bb_emit_byte(M, 0x15u);
    bb_emit_w32(M, addr);
    break;
  case GP_REG_ESI:
    bb_emit_byte(M, 0x66u); /* Operand-size Override Prefix - to indicate 16-bit mov */
    bb_emit_byte(M, 0x89u);
    bb_emit_byte(M, 0x35u);
    bb_emit_w32(M, addr);
    break;
  case GP_REG_EDI:
    bb_emit_byte(M, 0x66u); /* Operand-size Override Prefix - to indicate 16-bit mov */
    bb_emit_byte(M, 0x89u);
    bb_emit_byte(M, 0x3du);
    bb_emit_w32(M, addr);
    break;
  case GP_REG_EBP:
    bb_emit_byte(M, 0x66u); /* Operand-size Override Prefix - to indicate 16-bit mov */
    bb_emit_byte(M, 0x89u);
    bb_emit_byte(M, 0x2du);
    bb_emit_w32(M, addr);
    break;
  case GP_REG_ESP:
    bb_emit_byte(M, 0x66u); /* Operand-size Override Prefix - to indicate 16-bit mov */
    bb_emit_byte(M, 0x89u);
    bb_emit_byte(M, 0x25u);
    bb_emit_w32(M, addr);
    break;
  default:
    panic("bb_emit_save_reg_to() called with unknown register\n");
    break;
  }
}

INLINE void
bb_emit_restore_reg_from(machine_t *M, unsigned long whichReg, unsigned long addr)
{
  //bb_emit_byte(M, 0x65u); /* GS Segment Override Prefix - for accessing the M structure */

  switch(whichReg) {
  case GP_REG_EAX:
    bb_emit_byte(M, 0xa1u);
    bb_emit_w32(M, addr);
    break;
  case GP_REG_EBX:
    bb_emit_byte(M, 0x8bu);
    bb_emit_byte(M, 0x1du);
    bb_emit_w32(M, addr);
    break;
  case GP_REG_ECX:
    bb_emit_byte(M, 0x8bu);
    bb_emit_byte(M, 0x0du);
    bb_emit_w32(M, addr);
    break;
  case GP_REG_EDX:
    bb_emit_byte(M, 0x8bu);
    bb_emit_byte(M, 0x15u);
    bb_emit_w32(M, addr);
    break;
  case GP_REG_ESI:
    bb_emit_byte(M, 0x8bu);
    bb_emit_byte(M, 0x35u);
    bb_emit_w32(M, addr);
    break;
  case GP_REG_EDI:
    bb_emit_byte(M, 0x8bu);
    bb_emit_byte(M, 0x3du);
    bb_emit_w32(M, addr);
    break;
  case GP_REG_EBP:
    bb_emit_byte(M, 0x8bu);
    bb_emit_byte(M, 0x2du);
    bb_emit_w32(M, addr);
    break;
  case GP_REG_ESP:
    bb_emit_byte(M, 0x8bu);
    bb_emit_byte(M, 0x25u);
    bb_emit_w32(M, addr);
    break;
  default:
    panic("bb_emit_restore_reg_from() called with unknown register\n");
    break;
  }
}

INLINE void
bb_emit_save_reg(machine_t *M, unsigned long whichReg)
{
  unsigned long save_to;

  switch(whichReg) {
  case GP_REG_EAX:
    save_to = MREG(M, eax);    
    break;
  case GP_REG_EBX:
    save_to = MREG(M, ebx);    
    break;
  case GP_REG_ECX:
    save_to = MREG(M, ecx);    
    break;
  case GP_REG_EDX:
    save_to = MREG(M, edx);    
    break;
  case GP_REG_ESI:
    save_to = MREG(M, esi);    
    break;
  case GP_REG_EDI:
    save_to = MREG(M, edi);    
    break;
  case GP_REG_EBP:
    save_to = MREG(M, ebp);    
    break;
  case GP_REG_ESP:
    save_to = MREG(M, esp);    
    break;
  default:
    panic("bb_emit_save_reg() called with unknown register\n");
    break;
  }

  bb_emit_save_reg_to(M, whichReg, save_to);
}

INLINE void
bb_emit_restore_reg(machine_t *M, unsigned long whichReg)
{
  unsigned long restore_from;

  switch(whichReg) {
  case GP_REG_EAX:
    restore_from = MREG(M, eax);
    break;
  case GP_REG_EBX:
    restore_from = MREG(M, ebx);
    break;
  case GP_REG_ECX:
    restore_from = MREG(M, ecx);
    break;
  case GP_REG_EDX:
    restore_from = MREG(M, edx);
    break;
  case GP_REG_ESI:
    restore_from = MREG(M, esi);
    break;
  case GP_REG_EDI:
    restore_from = MREG(M, edi);
    break;
  case GP_REG_EBP:
    restore_from = MREG(M, ebp);
    break;
  case GP_REG_ESP:
    restore_from = MREG(M, esp);
    break;
  default:
    panic("bb_emit_restore_reg() called with unknown register\n");
    break;
  }

  bb_emit_restore_reg_from(M, whichReg, restore_from);
}

/* The following function is highly sensitive as the emitted code's size is used for computing something else */
INLINE void
bb_emit_store_immediate_to(machine_t *M, unsigned long imm, unsigned long dest)
{
  //bb_emit_byte(M, 0x65u); /* GS Segment Override Prefix - for accessing the M structure */
  bb_emit_byte(M, 0xc7u); /* mov immediate to memory */
  bb_emit_byte(M, 0x05u); /* 00 000 101 */
  bb_emit_w32(M, dest);   /* M dest */
  bb_emit_w32(M, imm);    /* imm32 */
}

/**************************************************************************************************************/

INLINE void
bb_emit_get_2_bytes_into_M(machine_t *M, decode_t *d, unsigned long addr)
{
  modrm_union modrm;
  unsigned reg = GP_REG_EAX;
  modrm.byte = (d->modrm).byte;

  if (modrm.parts.mod == 0x3u)
    {
      /* Required value is contained in register /modrm.parts.rm/. Save that register to the appropriate field of M */
      bb_emit_16_bit_save_reg_to(M, d->modrm.parts.rm, addr);
      return;
    }

  /* Free up the selected register to hold temporary */
  bb_emit_save_reg(M, reg);

  modrm.parts.reg = reg;
 
  if (d->flags & DSFL_GROUP2_PREFIX)
    bb_emit_byte(M, d->Group2_Prefix);
  
  bb_emit_byte(M, 0x66u);	/* Operand-size override prefix */

  if (d->flags & DSFL_GROUP4_PREFIX)
    bb_emit_byte (M, d->Group4_Prefix);

  bb_emit_byte(M, 0x8bu);	/* (16-bit) mov instruction */
  bb_emit_byte(M, modrm.byte);

  if (d->need_sib) 
    bb_emit_byte (M, d->sib.byte);

  switch(d->dispBytes) {
  case 1:
    bb_emit_byte(M, d->displacement);
    break;
  case 2:
    bb_emit_w16(M, d->displacement);
    break;
  case 4:
    bb_emit_w32(M, d->displacement);
    break;
  }

  /* Required value is now contained in register /reg/. Copy that
     to the appropriate field of M */
  bb_emit_16_bit_save_reg_to(M, reg, addr);

  /* Restore the register that we used as scratch register */
  bb_emit_restore_reg(M, reg);
}

INLINE void
bb_emit_get_4_bytes_into_M(machine_t *M, decode_t * d, unsigned long addr)
{
  modrm_union modrm;
  unsigned reg = GP_REG_EAX;
  modrm.byte = (d->modrm).byte;

  if (modrm.parts.mod == 0x3u)
    {
      /* Required value is contained in register /modrm.parts.rm/. Save that register to the appropriate field of M */
      bb_emit_save_reg_to(M, d->modrm.parts.rm, addr);
      return;
    }

  /* Free up the selected register to hold temporary */
  bb_emit_save_reg(M, reg);

  modrm.parts.reg = reg;
  
  if (d->flags & DSFL_GROUP2_PREFIX)
    bb_emit_byte (M, d->Group2_Prefix);
  
  if (d->flags & DSFL_GROUP4_PREFIX)
    bb_emit_byte (M, d->Group4_Prefix);

  bb_emit_byte(M, 0x8bu);	/* (32-bit) mov instruction */
  bb_emit_byte(M, modrm.byte);

  if (d->need_sib) 
    bb_emit_byte (M, d->sib.byte);

  switch(d->dispBytes) {
  case 1:
    bb_emit_byte(M, d->displacement);
    break;
  case 2:
    bb_emit_w16(M, d->displacement);
    break;
  case 4:
    bb_emit_w32(M, d->displacement);
    break;
  }

  /* Required value is now contained in register /reg/. Copy that
     to the appropriate field of M */
  bb_emit_save_reg_to(M, reg, addr);

  /* Restore the register that we used as scratch register */
  bb_emit_restore_reg(M, reg);
}

INLINE void
bb_emit_push_rm(machine_t *M, decode_t * d)
{
  modrm_union modrm;
  modrm.byte = (d->modrm).byte;

  modrm.parts.reg = 0x6u;
  
  if (d->flags & DSFL_GROUP2_PREFIX)
    bb_emit_byte (M, d->Group2_Prefix);
  
  if (d->flags & DSFL_GROUP4_PREFIX) { 
    bb_emit_byte (M, d->Group4_Prefix);
    panic("16 bit mode encountered\n");
  }

  /* Push FF /6*/
  bb_emit_byte(M, 0xFFu);
  bb_emit_byte(M, modrm.byte);

  if (d->need_sib) 
    bb_emit_byte (M, d->sib.byte);

  switch(d->dispBytes) {
  case 1:
    bb_emit_byte(M, d->displacement);
    break;
  case 2:
    bb_emit_w16(M, d->displacement);
    break;
  case 4:
    bb_emit_w32(M, d->displacement);
    break;
  }
}

INLINE void
bb_emit_lw_inc(machine_t *M, unsigned long addr) // [len 8b]
{
  bb_emit_byte(M, 0xFFu);  // INC FF/0
  bb_emit_byte(M, 0x05u);  // 00 000 101 
  bb_emit_w32(M, addr);
}

INLINE void
bb_emit_nop_inc(machine_t *M, unsigned long addr) // [len 8b]
{
  bb_emit_byte (M, 0x90u); // nop
  bb_emit_byte(M, 0xFFu);  // INC FF/0
  bb_emit_byte(M, 0x05u);  // 00 000 101 
  bb_emit_w32(M, addr);
  bb_emit_byte (M, 0x90u); // nop
}

INLINE void
bb_emit_inc(machine_t *M, unsigned long addr) // [len 8b]
{
  bb_emit_byte (M, 0x9Cu); //Pushf
  bb_emit_byte(M, 0xFFu);  // INC FF/0
  bb_emit_byte(M, 0x05u);  // 00 000 101 
  bb_emit_w32(M, addr);
  bb_emit_byte (M, 0x9Du); //Popf
}


INLINE void
bb_emit_lea_inc(machine_t *M, unsigned long addr)
{
  // Push %eax
  bb_emit_byte(M, 0x50u);
  
  // Mov (addr), %eax
  bb_emit_byte(M, 0x8bu); // 8b /r
  bb_emit_byte(M, 0x05u); // 00 000 101
  bb_emit_w32(M, addr);   
  
  // leal 1(%eax), %eax // 8D /r
  bb_emit_byte(M, 0x8du);
  bb_emit_byte(M, 0x40u); // 01 000 000
  bb_emit_byte(M, 0x01u);
  
  // mov %eax, (addr)
  bb_emit_byte(M, 0x89u); // 8b /r
  bb_emit_byte(M, 0x05u); // 00 000 101
  bb_emit_w32(M, addr);   

  // pop %eax
  bb_emit_byte(M, 0x58u);
}


/* Emit a call-back routine. Takes a pointer to the location holding
   the function pointer, and a single argument to be passed to the 
   emitted function */
INLINE void
bb_emit_call_back(machine_t *M, unsigned long fpp, unsigned long arg)
{
  bb_emit_byte(M, 0x9cu);		/* PUSHF */
  bb_emit_byte(M, 0x60u);		/* PUSHA */
  
  // Push arg
  bb_emit_byte(M, 0x68u);
  bb_emit_w32(M, arg);
  
  // call fpp
  bb_emit_call(M, (unsigned char *)fpp);
  
  // leal 4(%esp), %esp
  bb_emit_byte(M, 0x8du);
  bb_emit_byte(M, 0xA4u); /* 10 100 100 */
  bb_emit_byte(M, 0x24u); /* 00 100 100 */
  bb_emit_w32(M, 0x4u);

  bb_emit_byte(M, 0x61u);		/* POPA */
  bb_emit_byte(M, 0x9du);		/* POPF */
}

/* Emit a call-back routine. Takes a pointer to the location holding
   the function pointer, and 2 arguments to be passed to the 
   emitted function */
INLINE void
bb_emit_call_back3(machine_t *M, unsigned long fpp, 
		   unsigned long arg1,
		   unsigned long arg2,
		   unsigned long arg3)
{
  bb_emit_byte(M, 0x9cu);		/* PUSHF */
  bb_emit_byte(M, 0x60u);		/* PUSHA */

  // Push arg3
  bb_emit_byte(M, 0x68u);
  bb_emit_w32(M, arg3);

  // Push arg2
  bb_emit_byte(M, 0x68u);
  bb_emit_w32(M, arg2);

  // Push arg1
  bb_emit_byte(M, 0x68u);
  bb_emit_w32(M, arg1);
  
  // call fpp
  bb_emit_call(M, (unsigned char *)fpp);

  // leal 12(%esp), %esp
  bb_emit_byte(M, 0x8du);
  bb_emit_byte(M, 0xA4u); /* 10 100 100 */
  bb_emit_byte(M, 0x24u); /* 00 100 100 */
  bb_emit_w32(M, 0xCu);

  bb_emit_byte(M, 0x61u);		/* POPA */
  bb_emit_byte(M, 0x9du);		/* POPF */
}

