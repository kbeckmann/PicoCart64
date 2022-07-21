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
ECHO = echo

# To enable verbose prints, set VERBOSE=1 when building:
#   make VERBOSE=1
ifneq ($(strip $(VERBOSE)),1)
V = @
endif


C_SOURCES = \
	$(wildcard *.c) \
	$(wildcard ../../picocart64_shared/*.c)

C_INCLUDES = \
	-I$(ROOTDIR)/mips64-elf/include \
	-I$(BUILD_DIR) \
	-I../../picocart64_shared/include \

OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))

ASFLAGS = -mtune=vr4300 -march=vr4300
CFLAGS = -std=gnu99 -march=vr4300 -mtune=vr4300 -Wall $(C_INCLUDES)
LDFLAGS = -L$(ROOTDIR)/mips64-elf/lib -ldragon -lc -lm -ldragonsys -Tn64.ld --gc-sections
N64TOOLFLAGS = -l 1M -h $(ROOTDIR)/mips64-elf/lib/header -t "PicoCart64 Test ROM"

all: $(BUILD_DIR)/$(PROG_NAME).z64


$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(V)$(ECHO) "[  CC ]" $(notdir $<)
	$(V)$(CC) -o $@ $< -c $(CFLAGS) -Wa,-a,-ad,-alms=$(BUILD_DIR)/$(notdir $(<:.c=.lst)) 

$(BUILD_DIR)/$(PROG_NAME).z64: $(BUILD_DIR)/$(PROG_NAME).bin
	$(V)$(ECHO) "[ Z64 ]" $(notdir $@)
	$(V)$(N64TOOL) $(N64TOOLFLAGS) -o $@ $^
	$(V)$(ECHO) "[ CHK ]" $(notdir $@)
	$(V)$(CHKSUM64) $@

$(BUILD_DIR)/$(PROG_NAME).bin: $(BUILD_DIR)/$(PROG_NAME).elf
	$(V)$(ECHO) "[ BIN ]" $(notdir $@)
	$(V)$(OBJCOPY) $< $@ -O binary

$(BUILD_DIR)/$(PROG_NAME).elf: $(OBJECTS)
	$(V)$(ECHO) "[  LD ]" $(notdir $@)
	$(V)$(LD) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR):
	$(V)mkdir -p $@
	$(V)echo -n "#define GIT_REV 0x" > $(BUILD_DIR)/git_info.h
	$(V)git rev-parse --short=8 HEAD >> $(BUILD_DIR)/git_info.h
.PHONY: $(BUILD_DIR)

.PHONY: clean
clean:
	rm -rf ./build
