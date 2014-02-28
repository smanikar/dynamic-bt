##########################################################################
#
# Copyright (c) 2005, Johns Hopkins University and The EROS Group, LLC.
# All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
# 
#   * Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 
#   * Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in
#     the documentation and/or other materials provided with the
#     distribution.
# 
#   * Neither the name of the Johns Hopkins University, nor the name of
#     The EROS Group, LLC, nor the names of their contributors may be
#     used to endorse or promote products derived from this software
#     without specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
##########################################################################


# Check to see if we have ccache available. Use it if we do
ifndef CCACHE
ifneq "" "$(findstring /usr/bin/ccache,$(wildcard /usr/bin/ccache*))"
CCACHE=/usr/bin/ccache
endif
endif

LDFLAGS= -shared -Wl,-z,nodelete,-z,initfirst -Wl,--soname=vdebug.so.0
CC=$(CCACHE) gcc 
CPLUS=$(CCACHE) g++

#MAC_FLAGS= -mtune=athlon -mcpu=athlon
#MAC_FLAGS= -mtune=athlon64 -mcpu=athlon64
MAC_FLAGS=

OLD_CFLAGS= -fpic -O3 -funit-at-a-time $(MAC_FLAGS)
NEW_CFLAGS= -fpic -O3 -Winline -finline-limit=30000 -DINLINE_EMITTERS $(MAC_FLAGS)
NEW_CFLAGS+=--param inline-unit-growth=512 --param large-function-growth=2048
NEW_CFLAGS+=-funit-at-a-time
SGEN_CFLAGS= -fpic -DSTATIC_PASS -O3 -g -lm  $(MAC_FLAGS)

OLD_OBJECTS= UserEntry.o decode.o intel-decode.o emit.o emit-support.o
OLD_OBJECTS+= machine.o xlcore.o util.o disasm.o 
OLD_OBJECTS+= signals.o sig-names.o syscall-names.o

NEW_OBJECTS= UserEntry.o decode.o intel-decode.o emit.o
NEW_OBJECTS+= machine.o xlcore.o util.o disasm.o 
NEW_OBJECTS+= signals.o sig-names.o syscall-names.o

CODER_OBJECTS= decode-init.o decode-table.o decode.o emit.o 
CODER_OBJECTS+= emit-support.o util.o xlcore.o disasm.o
CODER_OBJECTS+= signals.o sig-names.o syscall-names.o 

SGEN_OBJECTS= decode.o emit.o emit-support.o disasm.o machine.o 
SGEN_OBJECTS+= util.o intel-decode.o disasm-driver.o xlcore.o 
SGEN_OBJECTS+= signals.o sig-names.o syscall-names.o

TESTER_OBJECTS= UserEntry.o decode.o intel-decode.o emit.o
TESTER_OBJECTS+= machine.o xlcore.o util.o disasm.o  
TESTER_OBJECTS+= signals.o sig-names.o syscall-names.o
TESTER_OBJECTS+= decode-tester.o

PERF_OBJECTS= perf.o

TARGETS= ovdebug.so nvdebug.so

#default: $(TARGETS)
default: new

all: $(TARGETS)

new: nvdebug.so

old: ovdebug.so

tester: decode-tester

%.new.o: %.c
	$(CC) -M $(NEW_CFLAGS) -o .tmp.m $<
	sed 's/\.o:/.new.o:/' .tmp.m > .$(@:.o=.m)
	rm -f .tmp.m
	$(CC) -c $(NEW_CFLAGS) -o $@ $<

%.new.o: %.s
	echo > .$(@:.o=.m)
	$(CC) -c $(NEW_CFLAGS) -o $@ $<

%.old.o: %.c
	$(CC) -M $(OLD_CFLAGS) -o .tmp.m $<
	sed 's/\.o:/.old.o:/' .tmp.m > .$(@:.o=.m)
	rm -f .tmp.m
	$(CC) -c $(OLD_CFLAGS) -o $@ $<

%.old.o: %.s
	echo > .$(@:.o=.m)
	$(CC) -c $(OLD_CFLAGS) -o $@ $<

%.sg.o: %.c
	$(CC) -M $(SGEN_CFLAGS) -o .tmp.m $<
	sed 's/\.o:/.sg.o:/' .tmp.m > .$(@:.o=.m)
	rm -f .tmp.m
	$(CC) -c $(SGEN_CFLAGS) -o $@ $<

%.sg.o: %.s
	echo > .$(@:.o=.m)
	$(CC) -c $(SGEN_CFLAGS) -o $@ $<

ovdebug.so: $(OLD_OBJECTS:.o=.old.o)
	$(CC) $(OLD_CFLAGS) $(LDFLAGS) $(OLD_OBJECTS:.o=.old.o) -o $@

nvdebug.so: $(NEW_OBJECTS:.o=.new.o)
	$(CC) $(NEW_CFLAGS) $(LDFLAGS) $(NEW_OBJECTS:.o=.new.o) -o $@

disasm-driver.sg.o: disasm-driver.c
	$(CPLUS) -c $(SGEN_CFLAGS) -o $@ $<

objects: $(OLD_OBJECTS:.o=.old.o) $(NEW_OBJECTS:.o=.new.o) $(SGEN_OBJECTS:.o=.sg.o)

sgen: $(SGEN_OBJECTS:.o=.sg.o)
	$(CPLUS) $(SGEN_CFLAGS) -o sgen $(SGEN_OBJECTS:.o=.sg.o) -lbfd -liberty -lsherpa

perf:	$(PERF_OBJECTS)
	$(CC) -O3 $(PERF_OBJECTS) -o perf

decode-tester:	$(TESTER_OBJECTS:.o=.new.o)
	$(CC) $(NEW_CFLAGS) $(TESTER_OBJECTS:.o=.new.o) -o $@

clean:
	-rm -f *.o $(TARGETS) *~ .*.m
	-rm -f intel-decode.tmp 
	-rm -rf new-intel-generator old-intel-generator sg-intel-generator 
	-rm -rf intel-decode.new.c intel-decode.old.c intel-decode.sg.c
	-rm -f disasm
	-rm -f perf
	-rm -f sgen

intel-decode.new.o: $(CODER_OBJECTS:.o=.new.o)
	$(CC) $(NEW_CFLAGS) -o new-intel-generator $(CODER_OBJECTS:.o=.new.o) -lm
	./new-intel-generator > intel-decode.new.c
	$(CC) -c $(NEW_CFLAGS) -o $@ intel-decode.new.c

intel-decode.old.o: $(CODER_OBJECTS:.o=.old.o)
	$(CC) $(OLD_CFLAGS)  -o old-intel-generator $(CODER_OBJECTS:.o=.old.o) -lm
	./old-intel-generator > intel-decode.old.c
	$(CC) -c $(OLD_CFLAGS) -o $@ intel-decode.old.c

intel-decode.sg.o: $(CODER_OBJECTS:.o=.sg.o)
	$(CC) $(SGEN_CFLAGS) -o sg-intel-generator $(CODER_OBJECTS:.o=.sg.o)
	./sg-intel-generator > intel-decode.sg.c
	$(CC) -c $(SGEN_CFLAGS) -o $@ intel-decode.sg.c

-include $(patsubst %.o,.%.m,$(NEW_OBJECTS:.o=.new.o))
-include $(patsubst %.o,.%.m,$(OLD_OBJECTS:.o=.old.o))
-include $(patsubst %.o,.%.m,$(SGEN_OBJECTS:.o=.sg.o))
-include $(patsubst %.o,.%.m,$(CODER_OBJECTS:.o=.new.o))
-include $(patsubst %.o,.%.m,$(CODER_OBJECTS:.o=.old.o))
-include $(patsubst %.o,.%.m,$(CODER_OBJECTS:.o=sg.o))
-include $(patsubst %.o,.%.m,$(PERF_OBJECTS))
