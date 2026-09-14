ROOTDIR := $(abspath $(dir $(shell which vasmm68k_mot))/..)

SOURCES = . asm ../../fixed

CPU ?= 020

ifeq ($(CPU),060)
ARCH = -m68060
AS_CPU = -m68060
else ifeq ($(CPU),040)
ARCH = -m68040
AS_CPU = -m68040
else ifeq ($(CPU),030)
ARCH = -m68030
AS_CPU = -m68030
else ifeq ($(CPU),020)
ARCH = -m68020
AS_CPU = -m68020
else
$(error Unsupported CPU=$(CPU). Use 020, 030, 040 or 060)
endif

ARCH += -msoft-float

SPECS_FLAG := -specs=softfloat.specs

PREFIX = $(ROOTDIR)/bin/m68k-amigaos-
CC = $(PREFIX)gcc
CXX = $(PREFIX)g++
CPP = $(PREFIX)cpp
AS = $(ROOTDIR)/bin/vasmm68k_mot
LD = $(PREFIX)ld
STRIP = $(PREFIX)strip

CC_VER := $(shell $(CC) -dumpversion)

#LIBPATH = -L$(ROOTDIR)/lib
INCPATH = -I. -I../../fixed

CFLAGS = $(ARCH) -noixemul -Wall -Wstack-usage=1024 -c
CFLAGS += -O2 -fbbb=- -fomit-frame-pointer -DNDEBUG -mno-bitfield

#CFLAGS += -g
CFLAGS += -DUSE_ASM
CFLAGS += -DUSE_MATRIX_INT16

CFLAGS += -DNO_FPS_CAP
CFLAGS += -DQUANT_ASM

#CFLAGS += -save-temps
#CFLAGS += -DPROFILING

CXXFLAGS = $(CFLAGS) -fno-rtti -fno-exceptions 
LDFLAGS = $(ARCH) -noixemul $(SPECS_FLAG) -Wl,-Map=$(BUILD)/output.map -fno-rtti -fno-exceptions
# -nostdlib
ASFLAGS = -quiet -I$(ROOTDIR)/m68k-amigaos/ndk-include -Fhunk -phxass $(AS_CPU) -opt-allbra -opt-brajmp -opt-clr -opt-div -opt-fconst -opt-jbra -opt-lsl -opt-movem -opt-mul -opt-nmoveq -opt-pea -opt-st -opt-speed -showcrit
DD = dd
RM = rm -f

BUILD = build_$(CPU)
TARGET = OpenLara.$(CPU)

FILES_AS = $(foreach dir, $(SOURCES), $(wildcard $(dir)/*.s))
FILES_CC = $(foreach dir, $(SOURCES), $(wildcard $(dir)/*.c))
FILES_XX = $(foreach dir, $(SOURCES), $(wildcard $(dir)/*.cpp))

LIBS = $(LIBPATH) -ldebug
# -lm
OBJS =  $(addprefix $(BUILD)/, $(notdir $(FILES_AS:%.s=%.o)))
OBJS += $(addprefix $(BUILD)/, $(notdir $(FILES_CC:%.c=%.o)))
OBJS += $(addprefix $(BUILD)/, $(notdir $(FILES_XX:%.cpp=%.o)))

.PHONY: dump clean

all: $(BUILD) $(TARGET)

dump:
	@[ -d dump ] || mkdir -p dump
	$(CXX) $(CFLAGS) $(INCPATH) -S -o dump/main.s main.cpp
	$(CXX) $(CFLAGS) $(INCPATH) -S -o dump/render.s render.cpp
	$(CXX) $(CFLAGS) $(INCPATH) -S -o dump/sound.s sound.cpp
	$(CXX) $(CFLAGS) $(INCPATH) -S -o dump/common.s ../../fixed/common.cpp

$(BUILD):
	@[ -d $@ ] || mkdir -p $@

$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) $(OBJS) $(LIBS) -o $(TARGET)
	$(STRIP) $(TARGET)
	python3 bss2hunk.py $(TARGET)

$(BUILD)/%.o: %.s | $(BUILD)
	$(AS) $(ASFLAGS) $(INCPATH) -o $@ $<

$(BUILD)/%.o: asm/%.s | $(BUILD)
	$(CPP) -P $(INCPATH) -o $(BUILD)/$(notdir $<) $<
	$(AS) $(ASFLAGS) $(INCPATH) -o $@ $(BUILD)/$(notdir $<)

$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) $(CFLAGS) $(INCPATH) -o $@ $<

$(BUILD)/%.o: %.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) $(INCPATH) -o $@ $<

$(BUILD)/main.o: main.cpp | $(BUILD)
	$(CXX) $(CFLAGS) -O0 -fno-rtti -fno-exceptions $(INCPATH) -o $@ $<

$(BUILD)/common.o: ../../fixed/common.cpp | $(BUILD)
	$(CXX) $(CFLAGS) -O0 -fno-rtti -fno-exceptions $(INCPATH) -o $@ $<

clean:
	$(RM) -r $(BUILD)
	$(RM) $(TARGET)
