CC := $(GENDEV)/m68k/bin/gcc
AS := $(GENDEV)/m68k/bin/as
RM := rm -f
OBJC := $(GENDEV)/m68k/bin/objcopy
ASMZ80 := $(GENDEV)/bin/zasm
BINTOC := $(GENDEV)/bin/bin2c

OBJS := crt0.o main.o neo2.o font.o pff.o diskio.o

OPTION := 
INCS := -I$(GENDEV)/m68k/include -I$(GENDEV)/m68k/sdk/include -I.
CCFLAGS := $(OPTION) -m68000 -Wall -O2 -c -fomit-frame-pointer $(INCS)
Z80FLAGS := -vb2
ASFLAGS := -m68000 --register-prefix-optional
LIBS := -L$(GENDEV)/m68k/lib -L$(GENDEV)/m68k/sdk/lib -lc -lgcc
LINKFLAGS := -T $(GENDEV)/m68k/lib/md-neo.ld
BUILD := build
TARGET := MDBIOS.BIN

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT  := $(CURDIR)/$(TARGET)
export VPATH   := $(CURDIR) $(CURDIR)/deluxe
export DEPENDS := $(OBJS:.o=.d)

.PHONY: $(BUILD) clean install

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@make --no-print-directory -C $(BUILD) -f $(CURDIR)/makefile

clean:
	@echo cleaning ...
	@rm -fr $(BUILD) $(OUTPUT)

install: $(BUILD)
	neocmd --menu $(OUTPUT)

all: $(BUILD)

else

ELF := $(notdir $(OUTPUT)).elf

$(OUTPUT): $(ELF)
	@$(OBJC) --pad-to 65536 -O binary $(ELF) $@

$(ELF): $(OBJS)
	@echo Linking $(TARGET)...
	@$(CC) $(LINKFLAGS) -nostdlib $(OBJS) $(LIBS) -o $(ELF)

%.o80: %.s80
	@echo Compiling $(notdir $<)...
	@$(ASMZ80) $(Z80FLAGS) -o $@ $<

%.c: %.o80
	@$(BINTOC) $<

%.o: %.c
	@echo Compiling $(notdir $<)...
	@$(CC) -MMD $(CCFLAGS) -c $< -o $@

%.o: %.s
	@echo Compiling $(notdir $<)...
	@$(AS) --MD $(@:.o=.d) $(ASFLAGS) $< -o $@

-include $(DEPENDS)

endif
