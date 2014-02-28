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

#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include "switches.h"
#include "debug.h"
#include "machine.h"
#include "decode.h"
#include "emit.h"
#include "util.h"

#define Do_decode_init(table, len) do_decode_init(F, table, #table, len);

/* It simplifies the inline decoder a good bit if we run through and
   precompute which opcodes require a modR/M byte. */
void
do_decode_init(FILE *F, const OpCode *table, char *name, unsigned length)
{
  unsigned i;
  fprintf(F, "\nCONST_TABLE OpCode %s [%u] = {\n", name, length);
  for (i = 0; i < length; i++) {
    unsigned a;
    const OpCode *op = &table[i];
    unsigned attr = op->attr;

    fprintf(F, "  { 0x%xu,  \"%s\",  ",op->index, op->disasm);

    for (a = 0; a < OP_MAXARG; a++) {
      switch(op->args[a].amode) {
      case ADDR_E:
      case ADDR_G:
      case ADDR_R:
      case ADDR_C:
      case ADDR_D:
      case ADDR_T:
	attr |= DF_MODRM;
	break;
      case ADDR_direct:
	attr |= DF_Ap;
	break;
      case ADDR_imm:
	{
	  switch(op->args[a].ainfo) {
	  case b_mode:
	    attr |= DF_Ib;
	    break;
	  case v_mode:
	    attr |= DF_Iv;
	    break;
	  case w_mode:
	    attr |= DF_Iw;
	    break;
	  default:
	    assert(0 && "unhandled immediate mode");
	  }
	  break;
	}
      case ADDR_offset:
	{
	  switch(op->args[a].ainfo) {
	  case b_mode:
	    attr |= DF_Ob;
	    break;
	  case v_mode:
	    attr |= DF_Ov;
	    break;
	  default:
	    assert(0 && "unhandled offset addr mode");
	  }
	  break;
	}
      case ADDR_jmp:
	{
	  switch(op->args[a].ainfo) {
	  case b_mode:
	    attr |= DF_Jb;
	    break;
	  case v_mode:
	    attr |= DF_Jv;
	    break;
	  default:
	    assert(0 && "unhandled jump addr mode");
	  }
	  break;
	}
      case ADDR_FREG:
	break;
      default:
	break;
      }

      fprintf(F, "%u, %u,  ",op->args[a].amode, op->args[a].ainfo);
    }

    fprintf(F, "%s,   \"%s\",  %u,  %u },\n",op->emitter_name, 
	    op->emitter_name, attr, op->flag_effect); 
  }
  fprintf(F, "};\n");
  fflush(stdout);
}

int
main()
{
  FILE *F = stdout;
  fprintf(F, "#include <stdbool.h>\n");
  fprintf(F, "#include \"switches.h\"\n");
  fprintf(F, "#include \"debug.h\"\n");
  fprintf(F, "#include \"machine.h\"\n");
  fprintf(F, "#include \"decode.h\"\n");
  fprintf(F, "#include \"emit.h\"\n");
  fprintf(F, "#define reserved 0\n");
  fprintf(F, "#define  prefix 0\n");

  Do_decode_init(group1_Eb_Ib, 8);
  Do_decode_init(group1_Ev_Iv, 8);
  Do_decode_init(group1_Ev_Ib, 8);
  Do_decode_init(group2a_Eb_Ib, 8);
  Do_decode_init(group2a_Ev_Ib, 8);
  Do_decode_init(group2_Eb_1, 8);
  Do_decode_init(group2_Ev_1, 8);
  Do_decode_init(group2_Eb_CL, 8);
  Do_decode_init(group2_Ev_CL, 8);
  Do_decode_init(group3b, 8);
  Do_decode_init(group3v, 8);
  Do_decode_init(ngroup4, 8);
  Do_decode_init(ngroup5, 8);
  Do_decode_init(ngroup6, 8);
  Do_decode_init(ngroup7, 8);
  Do_decode_init(group8_Ev_Ib, 8);
  Do_decode_init(ngroup9, 8);
  Do_decode_init(twoByteOpcodes, 256);
  Do_decode_init(nopbyte0, 256);
  Do_decode_init(float_d8, 16);
  Do_decode_init(float_d9, 16);
  Do_decode_init(float_d9_2, 8);
  Do_decode_init(float_d9_4, 8);
  Do_decode_init(float_d9_5, 8);
  Do_decode_init(float_d9_6, 8);
  Do_decode_init(float_d9_7, 8);
  Do_decode_init(float_da, 16);
  Do_decode_init(float_da_5, 8);
  Do_decode_init(float_db, 16);
  Do_decode_init(float_db_4, 8);
  Do_decode_init(float_dc, 16);
  Do_decode_init(float_dd, 16);
  Do_decode_init(float_de, 16);
  Do_decode_init(float_de_3, 8);
  Do_decode_init(float_df, 16);
  Do_decode_init(float_df_4, 8);

  fclose(F);
}
