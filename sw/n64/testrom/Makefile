PROG_NAME = testrom
BUILD_DIR = build

ROOTDIR = $(N64_INST)
GCCN64PREFIX = $(ROOTDIR)/bin/mips64-elf-

CC = $(GCCN64PREFIX)gcc
AS = $(GCCN64PREFIX)as
LD = $(GCCN64PREFIX)ld
OBJCOPY = $(GCCN64PREFIX)objcopy
N64TOOL = $(ROOTDIR)/bin/n64tool
CHKSUM64 = $(ROOTDIR)/bin/chksum64

C_SOURCES = \
	$(wildcard *.c) \
	$(wildcard ../../picocart64_shared/*.c)

C_INCLUDES = \
	-I$(ROOTDIR)/mips64-elf/include \
	-I. \
	-I../../picocart64_shared/include \

OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))

ASFLAGS = -mtune=vr4300 -march=vr4300
CFLAGS = -std=gnu99 -march=vr4300 -mtune=vr4300 -Wall $(C_INCLUDES)
LDFLAGS = -L$(ROOTDIR)/mips64-elf/lib -ldragon -lc -lm -ldragonsys -Tn64.ld --gc-sections
N64TOOLFLAGS = -l 1M -h $(ROOTDIR)/mips64-elf/lib/header -t "PicoCart64 Test ROM"

all: $(BUILD_DIR)/$(PROG_NAME).z64


$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) -o $@ $< -c $(CFLAGS) -Wa,-a,-ad,-alms=$(BUILD_DIR)/$(notdir $(<:.c=.lst)) 

$(BUILD_DIR)/$(PROG_NAME).z64: $(BUILD_DIR)/$(PROG_NAME).bin
	$(N64TOOL) $(N64TOOLFLAGS) -o $@ $^
	$(CHKSUM64) $@

$(BUILD_DIR)/$(PROG_NAME).bin: $(BUILD_DIR)/$(PROG_NAME).elf
	$(OBJCOPY) $< $@ -O binary

$(BUILD_DIR)/$(PROG_NAME).elf: $(OBJECTS)
	$(LD) -o $@ $^ $(LDFLAGS)

$(PROG_NAME).c:
	echo -n "#define GIT_REV 0x" > git_info.h
	git rev-parse --short=8 HEAD >> git_info.h
.PHONY: $(PROG_NAME).c

$(BUILD_DIR):
	mkdir $@

.PHONY: clean
clean:
	rm -f *.v64 *.z64 *.bin *.elf *.o
