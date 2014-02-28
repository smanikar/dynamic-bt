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
#include <stdbool.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <bfd.h>
#include <stdarg.h>
#include <string>
//#include <libsherpa/UExcept.hxx>
//#include <libsherpa/avl.hxx>

extern "C" {
#include "switches.h"
#include "debug.h"
#include "util.h"
#include "machine.h"
  machine_t * init_translator(unsigned long start);
  bb_entry * xlate_bb(machine_t *M);
  bb_entry * lookup_bb_eip(machine_t *M, unsigned long src_eip);
  void simple_patch(machine_t *M, unsigned long at, unsigned long addr);
  void dump_to_file(machine_t *M, char *str);
  void int_stub(machine_t *M);
};

using namespace sherpa;
typedef unsigned long ulong;
char *target = "i686-pc-linux-gnu";


void 
show_sections(bfd *abfd, asection *sec, void *nul)
{
  printf("%ld %10s Section starting at %08lx %s\n", sec->id, sec->name,
	 sec->vma, (sec->flags & SEC_CODE) ? "IS CODE" : "IS NOT CODE");
}

sec_mem *
get_into_mem(bfd *abfd)
{
  unsigned long sec_size;
  asection *sec;
  unsigned int i;
  sec_mem *sec_info = (sec_mem *) malloc(abfd->section_count * sizeof(sec_mem));
  for(sec = abfd->sections, i=0; sec != NULL ; sec = sec->next, i++) {
    sec_info[i].id = sec->id;
    sec_info[i].name = sec->name;
    sec_info[i].start = sec->vma;
    sec_size = bfd_section_size(abfd, sec); /* Assuming 1 octet per byte */
    sec_info[i].end =  sec->vma + sec_size;
    sec_info[i].inmem = (unsigned char *)malloc(sec_size);
    bfd_get_section_contents(abfd, sec, sec_info[i].inmem, 0, sec_size);
  }
  return sec_info;
}

struct patch_st {
  unsigned long at;
  unsigned long to;
  unsigned long proc_addr;
};

int
main(int argc, char **argv)
{
  int i;
  off_t fsize;
  int fd;
  bfd *abfd;
  asection *sec;
  char **matching;
  FILE *F;
  ulong addr;
  typedef struct patch_st patch_st;
  AvlTree<ulong> worklist;
  AvlNode<ulong> *neip;
  AvlMap<ulong, patch_st> patches;
  AvlMapNode<ulong, patch_st> *pat;

  if(argc != 2)
    panic("Usage: sgen <filename>");

  bfd_init();
  if(!bfd_set_default_target(target))
    panic("Failed to set target to i686-pc-linux-gnu");
  DEBUG(static_pass_addr_trans)
    printf("Trying %s\n", argv[1]);  
  abfd = bfd_openr(argv[1], target);

  if(abfd == NULL)
    panic("Could not openr %s\n", argv[1]);

  if(! bfd_check_format_matches(abfd, bfd_object, &matching))
    panic("%s - Not an Object File\n", argv[1]);

  if(abfd->sections == NULL)
    printf("NULL\n");


  if(bfd_check_format(abfd, bfd_archive))
    panic("Archives not yet supported. This is just an extra loop and will be done later");
  
  /* bfd_map_over_sections(abfd, show_sections, NULL); */
  
  /* Get Basic Block start addresses from the file bbaddrs */
  F = fopen("bbaddrs", "r");
  if(F == NULL)
    panic("No bbaddrs found\n");

  while(fscanf(F, "%lx", &addr) != EOF)
    worklist.insert(addr);
  unsigned long sec_size;
  unsigned long nsections = abfd->section_count;
  unsigned long start = bfd_get_start_address(abfd);
  machine_t *M = init_translator(start);
  M->nsections = nsections;
  M->sec_info = get_into_mem(abfd);
  /* This is technically not necessary. But I just thought of beginning
     the translation from the first instruction ... */
  xlate_bb(M);
  M->patch_count = 0;
  for(i=0; i<M->patch_count; i++) {
      worklist.insert((unsigned long) M->patch_array[i].to);
      patch_st *p = new patch_st;
      p->at = (unsigned long)M->patch_array[i].at;
      p->to = (unsigned long)M->patch_array[i].to;
      p->proc_addr = (unsigned long)M->patch_array[i].proc_addr;
      patches.insert((unsigned long)M->patch_array[i].to, *p); 
    }
  M->patch_count = 0;

  while((neip = worklist.least()) != NULL) {
    DEBUG(static_pass_addr_trans)
      printf("Translating %lx\n", neip->key); 
    
    M->fixregs.eip = (unsigned long) neip->key;
    worklist.remove(*neip);
    xlate_bb(M);    

    /* Add Contents of Patch_array to Worklist
       Add patch information to a AVL-Map */
    for(i=0; i<M->patch_count; i++) {
      worklist.insert((unsigned long) M->patch_array[i].to);
      patch_st *p = new patch_st;
      p->at = (unsigned long)M->patch_array[i].at;
      p->to = (unsigned long)M->patch_array[i].to;
      p->proc_addr = (unsigned long)M->patch_array[i].proc_addr;
      patches.insert((unsigned long)M->patch_array[i].to, *p); 
    }
    M->patch_count = 0;
  }
  
  /* Now patch all of the patch points together */
  while((pat = patches.least()) != NULL) {
    patch_st *p = &pat->value;
    bb_entry* curr_entry = lookup_bb_eip(M, p->to);
    if(curr_entry == NULL) {
      panic("WARNING: %lx Not found while Patching\n", p->to);
    }
    else {
      DEBUG(static_pass_addr_trans)
	printf("Patched %lx at %lx to %lx\n", p->to, p->at, curr_entry->trans_bb_eip);

      simple_patch(M, p->at, curr_entry->trans_bb_eip);
    }   
    patches.remove(*pat);    
  }
    
  /* Now dump the M structure */
  dump_to_file(M, argv[1]);

  /* If Necessary, get some statistics */
#ifdef PROFILE_BB_STATS
  int_stub(M);
#endif

  DEBUG(static_pass_addr_trans)
    printf("Done\n");

  
  bfd_close(abfd);
}
