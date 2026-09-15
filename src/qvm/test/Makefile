# The QuakeC VM's suite, native: qvm.c through a stand-in EMS that copies
# each page in and out of its window, against id's progs.dat out of the PAK
# and its tools/qcvm/qchash.py rewrite.
#
#   make            run everything
#   make mutate     every mutant of qvm.c and qchash.py must fail
#
# DOS: dosconf.sh stages the same test for Borland C++ 3.1 -mm and prints
# a conf to launch.
ROOT := $(abspath ../../..)
PAK  ?= $(HOME)/dos/QUAKE_SW/ID1/PAK0.PAK
B    ?= $(ROOT)/build/qvm-test
CC   ?= clang
QCVM := $(ROOT)/tools/qcvm

TEST_SRC := qvm_test.c qimg.c ems_nat.c ../qvm.c ../qstr.c
TEST_DEP := $(TEST_SRC) qimg.h ems.h ../qvm.h ../qvmi.h ../qtypes.h
LOAD_SRC := ed_load_test.c bspents.c ems_nat.c ../edict.c ../ed.c ../qvm.c ../qstr.c
LOAD_DEP := $(LOAD_SRC) bspents.h ems.h ../edict.h ../ed.h ../qvm.h ../qvmi.h ../qtypes.h
IMAGES   := $(B)/PROGS.DAT $(B)/PROGSH.DAT
MAPS     := $(patsubst %,$(B)/maps/%.bsp,e1m1 e1m2 e1m3 e1m4 e1m5 e1m6 e1m7 e1m8 start)
# QuakeSpasm's record of loading each: made by fixtures/quakespasm-eddump.patch, see its header
LOADS    := $(foreach m,e1m3 e1m4 start,$(B)/maps/$(m).bsp fixtures/$(m).dump)

.PHONY: all mutate

# a passing suite prints its last line; a failing one prints everything
all: $(B)/qvm_test $(B)/ems_nat_test $(B)/ed_test $(B)/ed_load_test $(IMAGES) $(MAPS)
	@$(B)/ems_nat_test > $(B)/ems_nat_test.txt || { cat $(B)/ems_nat_test.txt; exit 1; }
	@tail -1 $(B)/ems_nat_test.txt
	@$(B)/qvm_test $(IMAGES) > $(B)/qvm_test.txt || { cat $(B)/qvm_test.txt; exit 1; }
	@tail -1 $(B)/qvm_test.txt
	@$(B)/ed_test $(MAPS) > $(B)/ed_test.txt || { cat $(B)/ed_test.txt; exit 1; }
	@tail -1 $(B)/ed_test.txt
	@$(B)/ed_load_test $(B)/PROGSH.DAT $(LOADS) > $(B)/ed_load_test.txt || { cat $(B)/ed_load_test.txt; exit 1; }
	@tail -1 $(B)/ed_load_test.txt
	@python3 $(QCVM)/test_qchash.py $(B)/PROGS.DAT > $(B)/test_qchash.txt || { cat $(B)/test_qchash.txt; exit 1; }
	@tail -1 $(B)/test_qchash.txt

mutate: $(IMAGES) $(MAPS)
	@python3 mutate.py $(B)
	@python3 mut_edict.py $(B)
	@python3 $(QCVM)/mut_qchash.py $(B)/PROGS.DAT

$(B)/qvm_test: $(TEST_DEP) | $(B)
	@$(CC) -std=c89 -Wall -Werror -I.. -I. -o $@ $(TEST_SRC)

$(B)/ems_nat_test: ems_nat_test.c ems_nat.c ems.h ../qtypes.h | $(B)
	@$(CC) -std=c89 -Wall -Werror -I.. -I. -o $@ ems_nat_test.c ems_nat.c

$(B)/ed_test: ed_test.c idref.c bspents.c bspents.h ../ed.c ../ed.h ../qtypes.h | $(B)
	@$(CC) -std=c89 -Wall -Werror -Wno-misleading-indentation -Wno-comment -fsigned-char -I.. -I. -o $@ ed_test.c idref.c bspents.c ../ed.c

$(B)/ed_load_test: $(LOAD_DEP) | $(B)
	@$(CC) -std=c89 -Wall -Werror -I.. -I. -o $@ $(LOAD_SRC)

$(B)/maps/%.bsp: $(PAK) $(ROOT)/tools/pakget.py
	@mkdir -p $(@D)
	@python3 $(ROOT)/tools/pakget.py $(PAK) maps/$*.bsp $@

$(B)/PROGS.DAT: $(PAK) $(ROOT)/tools/pakget.py | $(B)
	@python3 $(ROOT)/tools/pakget.py $(PAK) progs.dat $@

$(B)/PROGSH.DAT: $(B)/PROGS.DAT $(QCVM)/qchash.py
	@python3 $(QCVM)/qchash.py $< $@ > /dev/null

$(B):
	@mkdir -p $@
