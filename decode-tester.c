#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include "switches.h"
#include "debug.h"
#include "machine.h"
#include "decode.h"
#include "emit.h"
#include "util.h"


machine_t IA32;
char instr_stream[] = {0xf2, 0x0f, 0x5c, 0xcd,
		       //0xc3, 0xc3, 0xc3, 0xc3,
		       0x66, 0x0f, 0x7e, 0xc2, 
		       0x0};

int
main()
{
  machine_t *M = &IA32;
  decode_t ds;
  decode_t *d = &ds;
  M->next_eip = (unsigned long)instr_stream;
  while(*((unsigned char *) M->next_eip)) {
    do_decode(M, d);
    do_disasm(d, stdout);
  }
  printf("0x%08lx:     THE END\n", M->next_eip);
}
