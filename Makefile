# NOTE: Recipe lines must start with a real TAB character.

# ---- Project config ----
PROJECT      := atmega64_PS2_interface  # change to your project name
MCU          := atmega64            # same for ATmega64L
F_CPU        := 8000000           # adjust to your clock
BAUD         := 250000
PROGRAMMER   := usbasp              # your programmer
AVRDUDE_MCU  := m64                 # avrdude device id for ATmega64/64L
# If target clock is slow (e.g., internal RC at low freq), uncomment to slow SCK:
AVRDUDE_SCK := -B 10

# ---- Tools ----
CC           := avr-gcc
OBJCOPY      := avr-objcopy
OBJDUMP      := avr-objdump
SIZE         := avr-size
AVRDUDE      := avrde

# ---- Paths ----
SRC_DIR      := src
LIB_ROOT_DIR := library
BUILD_DIR    := build

# Automatically find all subdirectories under 'library/'
# This assumes each library resides in its own subdirectory directly under 'library/'
LIB_SUBDIRS  := $(wildcard $(LIB_ROOT_DIR)/*)

# All directories where the compiler should search for header files
# (src/ if main.c has its own header, and all library subdirectories)
ALL_INCLUDE_DIRS := $(SRC_DIR) $(LIB_SUBDIRS)

# ---- Sources ----
# Source files from the main src directory
MAIN_SRCS    := $(wildcard $(SRC_DIR)/*.c)
# Source files from all library subdirectories
LIB_SRCS     := $(foreach dir, $(LIB_SUBDIRS), $(wildcard $(dir)/*.c))

# Combine all source files
ALL_SRCS     := $(MAIN_SRCS) $(LIB_SRCS)

# Generate a list of object files, placing them all in the BUILD_DIR.
# We use $(notdir ...) to get just the filename, avoiding path conflicts in build/
OBJ          := $(patsubst %.c,$(BUILD_DIR)/%.o,$(notdir $(ALL_SRCS)))
DEP          := $(OBJ:.o=.d)

# ---- Flags ----
CSTD         := gnu11
# Transform ALL_INCLUDE_DIRS into -I flags for the compiler
INC_FLAGS    := $(addprefix -I, $(ALL_INCLUDE_DIRS))

CFLAGS       := -mmcu=$(MCU) -DF_CPU=$(F_CPU) -DBAUD=$(BAUD) -std=$(CSTD) -Os \
          -Wall -Wextra -Wundef -Wstrict-prototypes -fdata-sections -ffunction-sections \
          -MMD -MP $(INC_FLAGS) # INC_FLAGS now handles all include paths
LDFLAGS      := -mmcu=$(MCU) -Wl,--gc-sections
AVRDUDE_FLAGS:= -p $(AVRDUDE_MCU) -c $(PROGRAMMER) $(AVRDUDE_SCK)

# ---- VPATH (Virtual Path) ----
# make will look for prerequisites (like .c files) in these directories
VPATH        := $(SRC_DIR):$(LIB_SUBDIRS)

# ---- Targets ----
.PHONY: all flash fuses fuse-read clean size disasm

all: $(BUILD_DIR)/$(PROJECT).hex

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

# Pattern rule for compiling any .c file into a .o file in the build directory.
# VPATH will ensure make finds the correct .c file in src/ or library/ subdirectories.
$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/$(PROJECT).elf: $(OBJ)
	$(CC) $(LDFLAGS) $^ -o $@

$(BUILD_DIR)/$(PROJECT).hex: $(BUILD_DIR)/$(PROJECT).elf
	$(OBJCOPY) -O ihex -R .eeprom $< $@
	@$(SIZE) -C --mcu=$(MCU) $(BUILD_DIR)/$(PROJECT).elf

size: $(BUILD_DIR)/$(PROJECT).elf
	$(SIZE) -C --mcu=$(MCU) $<

disasm: $(BUILD_DIR)/$(PROJECT).elf
	$(OBJDUMP) -d $< > $(BUILD_DIR)/$(PROJECT).lst
	@echo "Disassembly written to $(BUILD_DIR)/$(PROJECT).lst"

flash: $(BUILD_DIR)/$(PROJECT).hex
	$(AVRDUDE) $(AVRDUDE_FLAGS) -U flash:w:$<:i

# Read back fuse bytes (hex). ATmega64 has low and high fuses.
fuse-read:
	$(AVRDUDE) $(AVRDUDE_FLAGS) -U lfuse:r:-:h -U hfuse:r:-:h -U efuse:r:-:h

# WRITE FUSES CAREFULLY — fill in values from a fuse calculator for your clock/setup.
# Example placeholders shown below; replace 0x?? with your values.
fuses:
	@echo "Edit the 'fuses' target in the Makefile with correct values before using!"
	$(AVRDUDE) $(AVRDUDE_FLAGS) -U lfuse:w:0xe4:m -U hfuse:w:0xd8:m

clean:
	@rm -rf $(BUILD_DIR)

-include $(DEP)
