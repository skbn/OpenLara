ROOTDIR := $(abspath $(dir $(shell which vasmm68k_mot))/..)

TARGET = OpenLara
BUILD = build
SOURCES = . asm ../../fixed

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

CFLAGS = -m68020 -noixemul -Wall -Wstack-usage=1024 -c
CFLAGS += -O2 -fbbb=- -fomit-frame-pointer -DNDEBUG -mno-bitfield

#CFLAGS += -g
CFLAGS += -DUSE_ASM
CFLAGS += -DUSE_MATRIX_INT16

CFLAGS += -DNO_FPS_CAP
CFLAGS += -DQUANT_ASM

#CFLAGS += -save-temps
#CFLAGS += -DPROFILING

CXXFLAGS = $(CFLAGS) -fno-rtti -fno-exceptions 
LDFLAGS = -m68020 -noixemul -Wl,-Map=$(BUILD)/output.map -fno-rtti -fno-exceptions
# -nostdlib
ASFLAGS = -quiet -I$(ROOTDIR)/m68k-amigaos/ndk-include -Fhunk -phxass -m68020 -opt-allbra -opt-brajmp -opt-clr -opt-div -opt-fconst -opt-jbra -opt-lsl -opt-movem -opt-mul -opt-nmoveq -opt-pea -opt-st -opt-speed -showcrit
DD = dd
RM = rm -f

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
	$(RM) $(BUILD)/* $(TARGET)
