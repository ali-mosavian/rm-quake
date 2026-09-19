##
## qcport -- Quake for real-mode DOS. C against Borland C 2.01's
## medium-model runtime (C0M.OBJ, CM.LIB, MATHM.LIB) plus qgl's objects.
##
## qgl assembles natively with jwasm and without __BASIC__. Each C
## compile is its own DOSBox-X session (tools/bcc.sh), so `make -jN`
## parallelises; the link needs every object at once and stays single.
##

TOOLCHAINS ?= $(HOME)/work/other/d32x/toolchains
DOSBOX_BIN ?=
BUILD      ?= $(CURDIR)/build/cport
JWASM      ?= $(TOOLCHAINS)/native/bin/jwasm
QGL        := $(CURDIR)/src/qgl
TIMEOUT    ?= 600
# Quake's shareware PAK: the maps, models, sounds and status bar come out
# of it. Not in the repo and not redistributable.
PAK        ?= $(HOME)/dos/QUAKE_SW/ID1/PAK0.PAK
# the test map the gates and qgl's suite read
MAP        ?= dm3ish.bsp
# the maps a run chains through
MAPS       ?= e1m1 e1m2 e1m3 e1m4 e1m5 e1m6 e1m7 e1m8 start

# file.asm FIRST: a VFS driver registers by placing a row in a segment
# file.asm brackets, and TLINK lays the class out in the order it meets
# the modules. Same order src/qgl/test/Makefile uses.
QGL_SRC := $(QGL)/file.asm $(QGL)/vga.asm \
           $(QGL)/sf.asm $(QGL)/mem.asm $(QGL)/gem.asm \
           $(QGL)/qglnew.asm $(QGL)/qglview.asm $(QGL)/qgldc.asm \
           $(QGL)/qglrow.asm $(QGL)/dct/dctmem.asm $(QGL)/dct/dctems.asm \
           $(QGL)/txt.asm $(QGL)/dr.asm \
           $(QGL)/z.asm $(QGL)/clip.asm $(QGL)/rs.asm $(QGL)/sb.asm \
           $(QGL)/ar.asm $(QGL)/dsp.asm \
           $(QGL)/log.asm $(QGL)/tmr.asm $(QGL)/kbd.asm $(QGL)/mouse.asm \
           $(QGL)/m4.asm \
           $(QGL)/b8/b8fill.asm $(QGL)/b8/b8span.asm
# Not linked: cpi4.asm replaces VBDOS runtime routines and calls that
# runtime's __aFldiv/__aFlrem, and zip.asm is a container driver the .qmp
# does not need -- file.asm reads its members by offset.
QGL_MODS := $(basename $(notdir $(QGL_SRC)))
QGL_INC  := $(wildcard $(QGL)/*.inc)

# -D list for every C compile -- see src/host/qrcfg.h. Nothing in make sees a
# changed define, so an arm built with a different CDEFS needs its own
# BUILD directory:
#
#   make BUILD=$PWD/build/cport-noprof CDEFS="-DQR_PROF=0 -DQR_LOG=0"
#   make BUILD=$PWD/build/cport-capture CDEFS="-DQR_DEBUG_CAPTURE=1"
CDEFS ?=

export TOOLCHAINS DOSBOX_BIN CDEFS TIMEOUT

BCC  := $(CURDIR)/tools/bcc.sh
LINK := $(CURDIR)/tools/link.sh

# The program's subsystems. src/qgl is qgl, built by the rules below.
C_DIRS := $(addprefix src/,host render model game sound ui)
C_SRC  := $(foreach d,$(C_DIRS),$(wildcard $(d)/*.c))
C_MODS := $(sort $(basename $(notdir $(C_SRC))))

# Standalone .asm modules (r_ptproj.asm: r_ptproj/r_rclip), assembled
# natively with jwasm.
ASM_SRC  := $(foreach d,$(C_DIRS),$(wildcard $(d)/*.asm))
ASM_MODS := $(sort $(basename $(notdir $(ASM_SRC))))

MODS := $(C_MODS) $(ASM_MODS) $(QGL_MODS)
# Objects land flat in BUILD and bcc.sh compiles against every header in
# one flat DOS directory, so a name may live in one subsystem only.
# from the unsorted names: $(sort) above drops a repeat before this could see it
DUPS := $(shell printf '%s\n' $(basename $(notdir $(C_SRC) $(ASM_SRC))) $(QGL_MODS) $(notdir $(wildcard src/*/*.h)) | tr A-Z a-z | sort | uniq -d)
$(if $(DUPS),$(error one name in two subsystems: $(DUPS)))
OBJS := $(addprefix $(BUILD)/,$(addsuffix .obj,$(MODS)))
EXE  := $(BUILD)/QCPORT.EXE

.PHONY: all build title assets maps test run clean help

# ONE file per map: <map>.qmp carries every lump, both atlases, the
# palette, the font and the colormap, and the run takes its name.
DATA_SRC := $(CURDIR)/data/assets
DATA     := $(basename $(MAP)).qmp
# stuff.ini is read by name out of the working directory -- it is the
# run's configuration, not the map's, and config.c opens it with plain
# fopen.
ROOT_DATA := stuff.ini default.cfg
DATA_OUT := $(addprefix $(BUILD)/,$(DATA) $(ROOT_DATA))

all: build                      ## build QCPORT.EXE (default)
build: $(EXE) $(DATA_OUT)

# The no-argument id-style attract loop. The camera tracks are extracted
# from id's protocol-15 demos in the shareware PAK.
TITLE_MAPS := start e1m3 e1m4 e1m6
TITLE_DEMOS := demo1.qdc demo2.qdc demo3.qdc
TITLE_MAP_OUT := $(foreach m,$(TITLE_MAPS),$(BUILD)/$(m).qmp)
TITLE_DEMO_OUT := $(addprefix $(BUILD)/,$(TITLE_DEMOS))
title: build $(TITLE_MAP_OUT) $(TITLE_DEMO_OUT) ## build the no-argument title-demo setup

$(foreach m,$(TITLE_MAPS),$(eval $(BUILD)/$(m).qmp: data/maps/$(m)/$(m).qmp | $(BUILD); cp $$< $$@))
$(BUILD)/demo%.qdc: $(PAK) tools/mkidcam.py tools/mdlsets.py tools/mksnd.py tools/pakget.py | $(BUILD)
	@python3 tools/mkidcam.py $(PAK) demo$*.dem $@

# Named one by one, not a `$(BUILD)/%:` pattern -- that pattern also
# matches $(BUILD)/<anything>.obj and would compete with the compile
# rules for every object.
$(foreach f,$(DATA),$(eval $(BUILD)/$(f): $(DATA_SRC)/$(f) | $(BUILD); cp $$< $$@))
$(foreach f,$(ROOT_DATA),$(eval $(BUILD)/$(f): $(CURDIR)/data/$(f) | $(BUILD); cp $$< $$@))

$(BUILD):
	mkdir -p $(BUILD)

# On every header, not just the .c: without this an edit to qgl.h or a
# struct changes what the objects mean while leaving the sources
# untouched, LINK is happy, and the EXE runs old declarations against
# new code.
C_HDRS := $(foreach d,$(C_DIRS) $(QGL),$(wildcard $(d)/*.h))

vpath %.c $(C_DIRS)
vpath %.asm $(C_DIRS)

$(BUILD)/%.obj: %.c $(C_HDRS) | $(BUILD)
	$(BCC) $< $@

$(BUILD)/%.obj: %.asm | $(BUILD)
	$(JWASM) -c -Cp -Zg -omf -Fo$@ $<

# No __BASIC__ here, deliberately. On every .inc: qgl.inc carries the
# surface kinds and the struct layouts, so an edit there changes what
# these objects mean while leaving every .asm file untouched.
$(BUILD)/%.obj: $(QGL)/%.asm $(QGL_INC) | $(BUILD)
	$(JWASM) -c -Cp -Zg -omf -I$(QGL) -Fo$@ $<

$(BUILD)/%.obj: $(QGL)/b8/%.asm $(QGL_INC) | $(BUILD)
	$(JWASM) -c -Cp -Zg -omf -I$(QGL) -Fo$@ $<

$(BUILD)/%.obj: $(QGL)/dct/%.asm $(QGL_INC) | $(BUILD)
	$(JWASM) -c -Cp -Zg -omf -I$(QGL) -Fo$@ $<

# The header is generated, and a stale one miscompiles rather than
# failing: C will not catch a drifted declaration.
.PHONY: qglh
qglh:
	@python3 $(CURDIR)/tools/mkqglh.py $(QGL) $(QGL)/qgl.h --check \
	  || { python3 $(CURDIR)/tools/mkqglh.py $(QGL) $(QGL)/qgl.h; \
	       echo "qgl.h was stale and has been regenerated -- rebuild" >&2; \
	       exit 1; }

$(OBJS): | qglh

$(EXE): $(OBJS)
	$(LINK) $(BUILD) "$(MODS)"

# dm3ish: assets.zip for qgl's suite, dm3ish.qmp for the gates.
ASSETS     := data/assets/assets.zip
MAP_ROOT   ?= data/maps
MAP_ASSETS := $(if $(wildcard $(PAK)),$(foreach m,$(MAPS),$(MAP_ROOT)/$(m)/assets.zip))
MKASSETS   := tools/mkassets.py tools/mdlsets.py tools/lighthash.py tools/mkportals.py tools/mksnd.py tools/texquant.py tools/fsdither.c \
              tools/mkmdl.py tools/mksprite.py tools/mkgfx.py tools/mdlview.py
MKASSETS_CMD ?= python3 tools/mkassets.py

assets: $(ASSETS)               ## dm3ish's assets and .qmp
maps: $(MAP_ASSETS)             ## e1m1..e1m8 and start, a .qmp each

# the .qmp is written by the same run: without this the build copied one
# older than mkassets.py
$(DATA_SRC)/$(DATA): $(ASSETS) ;

$(ASSETS): data/$(MAP) data/base.dat $(MKASSETS)
	@python3 tools/mkassets.py data/$(MAP) data/base.dat data/assets $(PAK)

$(foreach m,$(MAPS),data/$(m).bsp): data/%.bsp: $(PAK) tools/pakget.py
	@python3 tools/pakget.py $(PAK) maps/$*.bsp $@

$(MAP_ROOT)/%/assets.zip: data/%.bsp data/base.dat $(MKASSETS)
	@$(MKASSETS_CMD) data/$*.bsp data/base.dat $(MAP_ROOT)/$* $(PAK)

# mkassets writes both outputs in one pass. Name the QMP side too, or title
# can copy an old container even after a generator changed the ZIP stamp.
define MAP_QMP_RULE
$(MAP_ROOT)/$(1)/$(1).qmp: $(MAP_ROOT)/$(1)/assets.zip
	@if [ ! -f "$$@" ] || [ "$$<" -nt "$$@" ]; then \
	  $(MKASSETS_CMD) data/$(1).bsp data/base.dat $(MAP_ROOT)/$(1) $(PAK); \
	fi
endef
$(foreach m,$(MAPS),$(eval $(call MAP_QMP_RULE,$(m))))

test:                           ## header deps, far-to-near pointers, then qgl's suite, native; needs `make assets`
	@tools/depcheck.sh
	@tools/test-dups.sh
	@tools/test-assetdeps.sh
	@tools/test-console.sh
	@tools/test-keybind.sh
	@tools/test-centerprint.sh
	@tools/test-video.sh
	@tools/test-viewfx.sh
	@tools/test-viewshift.sh
	@tools/test-viewmdl.sh
	@tools/test-particles.sh
	@tools/test-pl-trace.sh
	@python3 tools/test-lighthash.py
	@python3 tools/test-mksnd.py
	@python3 tools/test-mkidcam-lightning.py
	@python3 tools/test-id-demo-camera.py $(PAK)
	@python3 tools/test-sprites.py $(PAK)
	@tools/test-sprite-draw.sh $(BUILD)
	@python3 tools/farnear.py
	@tools/test-x87cmp.sh
	@tools/test-bufcheck.sh
	@$(MAKE) --no-print-directory -C src/qgl/test

run: $(EXE)                     ## headless run; cport.txt/cstep.txt land in $(BUILD)
	@tools/run.sh $(BUILD)

clean:                          ## drop all build output
	rm -rf $(BUILD)

help:
	@grep -hE '^[a-z0-9]+:.*##' $(MAKEFILE_LIST) | sed 's/:.*##/\t/' | expand -t 14
