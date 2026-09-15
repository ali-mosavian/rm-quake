# The QuakeC VM's suite, native: qvm.c through a stand-in EMS that copies
# each page in and out of its window, against id's progs.dat out of the PAK
# and its tools/qcvm/qchash.py rewrite.
#
#   make            run everything
#   make mutate     every mutant of qvm.c and qchash.py must fail
#
# DOS: dosconf.sh stages the same test for Borland C++ 3.1 -mm and prints
# a conf to launch.
ROOT := $(abspath ../..)
PAK  ?= $(HOME)/dos/QUAKE_SW/ID1/PAK0.PAK
B    ?= $(ROOT)/build/qvm-test
CC   ?= clang
QCVM := $(ROOT)/tools/qcvm

TEST_SRC := qvm_test.c qimg.c ems_nat.c ../qvm.c
TEST_DEP := $(TEST_SRC) qimg.h ems.h ../qvm.h ../qtypes.h
IMAGES   := $(B)/PROGS.DAT $(B)/PROGSH.DAT

.PHONY: all mutate

# a passing suite prints its last line; a failing one prints everything
all: $(B)/qvm_test $(B)/ems_nat_test $(IMAGES)
	@$(B)/ems_nat_test > $(B)/ems_nat_test.txt || { cat $(B)/ems_nat_test.txt; exit 1; }
	@tail -1 $(B)/ems_nat_test.txt
	@$(B)/qvm_test $(IMAGES) > $(B)/qvm_test.txt || { cat $(B)/qvm_test.txt; exit 1; }
	@tail -1 $(B)/qvm_test.txt
	@python3 $(QCVM)/test_qchash.py $(B)/PROGS.DAT > $(B)/test_qchash.txt || { cat $(B)/test_qchash.txt; exit 1; }
	@tail -1 $(B)/test_qchash.txt

mutate: $(IMAGES)
	@python3 mutate.py $(B)
	@python3 $(QCVM)/mut_qchash.py $(B)/PROGS.DAT

$(B)/qvm_test: $(TEST_DEP) | $(B)
	@$(CC) -std=c89 -Wall -Werror -I.. -I. -o $@ $(TEST_SRC)

$(B)/ems_nat_test: ems_nat_test.c ems_nat.c ems.h ../qtypes.h | $(B)
	@$(CC) -std=c89 -Wall -Werror -I.. -I. -o $@ ems_nat_test.c ems_nat.c

$(B)/PROGS.DAT: $(PAK) $(ROOT)/tools/pakget.py | $(B)
	@python3 $(ROOT)/tools/pakget.py $(PAK) progs.dat $@

$(B)/PROGSH.DAT: $(B)/PROGS.DAT $(QCVM)/qchash.py
	@python3 $(QCVM)/qchash.py $< $@ > /dev/null

$(B):
	@mkdir -p $@
