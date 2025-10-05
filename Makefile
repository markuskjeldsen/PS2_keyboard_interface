# NOTE: Recipe lines must start with a real TAB character.

# ---- Project config ----
PROJECT      := atmega64_blink
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
AVRDUDE      := avrdude

# ---- Paths ----
# The main source directory for your application code
PROJECT_SRC_DIR := src
# Directory for common, project-wide headers (optional, but good practice)
INC_DIR      := include
# Directory for build artifacts (.o, .d files)
BUILD_DIR    := build
# Directory for final output files (.elf, .hex)
BIN_DIR      := bin

# ---- Library Configuration (Expandable) ----
# List all directories containing library source and header files.
# To add a new library module, create its directory (e.g., library/spi)
# and add it to this list. Headers in these directories will also be
# automatically added to the include path.
LIBRARY_SRC_DIRS := library/ps2 \
                    library/uart
# NOTE: If you add `library/spi` here, it will automatically find `library/spi/spi.c`
# and add `library/spi` to the compiler's include search paths.

# ---- Sources and Objects ----
# Combine project source directory and library source directories for source discovery
ALL_SOURCE_SEARCH_DIRS := $(PROJECT_SRC_DIR) $(LIBRARY_SRC_DIRS)

# Find all C source files in the specified directories
# SRCS will contain paths like: src/main.c library/ps2/ps2.c library/uart/uart.c
SRCS := $(foreach dir, $(ALL_SOURCE_SEARCH_DIRS), $(wildcard $(dir)/*.c))

# Generate object file paths within the build directory, preserving subdirectory structure
# e.g., src/main.c -> build/src/main.o
# e.g., library/ps2/ps2.c -> build/library/ps2/ps2.o
OBJS := $(patsubst %.c, $(BUILD_DIR)/%.o, $(SRCS))

# Generate dependency file paths (used by -MMD/-MP)
DEPS := $(patsubst %.c, $(BUILD_DIR)/%.d, $(SRCS))

# ---- Flags ----
CSTD         := gnu11
# Compiler flags:
# -mmcu=$(MCU)           : Specify microcontroller
# -DF_CPU=$(F_CPU)       : Define clock frequency
# -DBAUD=$(BAUD)         : Define baud rate for UART (if used)
# -std=$(CSTD)           : C standard (e.g., C11)
# -Os                    : Optimize for size
# -Wall -Wextra          : Enable all common warnings
# -Wundef -Wstrict-prototypes -fdata-sections -ffunction-sections : More warnings and optimization
# -MMD -MP               : Generate dependency files for efficient incremental builds
# -I$(INC_DIR)           : Include path for project-wide headers
# -I$(dir)               : Include paths for library-specific headers
ALL_INCLUDE_FLAGS := -I$(INC_DIR) $(foreach dir, $(LIBRARY_SRC_DIRS), -I$(dir)) $(if $(wildcard $(PROJECT_SRC_DIR)), -I$(PROJECT_SRC_DIR))
CFLAGS := -mmcu=$(MCU) -DF_CPU=$(F_CPU) -DBAUD=$(BAUD) -std=$(CSTD) -Os \
          -Wall -Wextra -Wundef -Wstrict-prototypes -fdata-sections -ffunction-sections \
          -MMD -MP $(ALL_INCLUDE_FLAGS)

# Linker flags:
# -mmcu=$(MCU)           : Specify microcontroller
# -Wl,--gc-sections      : Garbage collect unused sections for smaller code size
LDFLAGS      := -mmcu=$(MCU) -Wl,--gc-sections

# AVRDUDE flags:
# -p $(AVRDUDE_MCU)      : Specify device ID for avrdude
# -c $(PROGRAMMER)       : Specify programmer type
# $(AVRDUDE_SCK)         : Optional: Slow down SCK if needed
AVRDUDE_FLAGS:= -p $(AVRDUDE_MCU) -c $(PROGRAMMER) $(AVRDUDE_SCK)

# ---- Targets ----
.PHONY: all flash fuses fuse-read clean size disasm rebuild

# Default target: build the program
all: $(BIN_DIR)/$(PROJECT).hex

# Ensure build and bin directories exist
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)
$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

# Rule to compile each C source file into an object file
# This rule dynamically creates subdirectories in BUILD_DIR if necessary
# e.g., creates build/src, build/library/ps2, etc.
$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	@mkdir -p $(@D) # Create necessary subdirectories in build/
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Rule to link the object files into the final ELF executable
$(BIN_DIR)/$(PROJECT).elf: $(OBJS) | $(BIN_DIR)
	@echo "Linking $(PROJECT).elf..."
	$(CC) $(LDFLAGS) $^ -o $@

# Rule to convert the ELF file to an Intel HEX file
$(BIN_DIR)/$(PROJECT).hex: $(BIN_DIR)/$(PROJECT).elf
	@echo "Creating $(PROJECT).hex..."
	$(OBJCOPY) -O ihex -R .eeprom $< $@
	@echo "Memory usage for $(PROJECT):"
	@$(SIZE) -C --mcu=$(MCU) $<

# Display memory usage (flash, RAM)
size: $(BIN_DIR)/$(PROJECT).elf
	@echo "Memory usage for $(PROJECT):"
	$(SIZE) -C --mcu=$(MCU) $<

# Generate disassembly listing
disasm: $(BIN_DIR)/$(PROJECT).elf
	@echo "Generating disassembly in $(BIN_DIR)/$(PROJECT).lst..."
	$(OBJDUMP) -d $< > $(BIN_DIR)/$(PROJECT).lst

# Flash the program to the microcontroller
flash: $(BIN_DIR)/$(PROJECT).hex
	@echo "Flashing $(PROJECT).hex to $(MCU) via $(PROGRAMMER)..."
	$(AVRDUDE) $(AVRDUDE_FLAGS) -U flash:w:$<:i

# Read back fuse bytes (hex). ATmega64 has low, high, and extended fuses.
fuse-read:
	@echo "Reading fuse bytes from $(MCU) via $(PROGRAMMER)..."
	$(AVRDUDE) $(AVRDUDE_FLAGS) -U lfuse:r:-:h -U hfuse:r:-:h -U efuse:r:-:h

# WRITE FUSES CAREFULLY — fill in values from a fuse calculator for your clock/setup.
# Example placeholders shown below; replace 0x?? with your values.
fuses:
	@echo "----------------------------------------------------------------------"
	@echo "WARNING: Editing fuses can brick your MCU if done incorrectly."
	@echo "         Edit the 'fuses' target in the Makefile with correct values!"
	@echo "----------------------------------------------------------------------"
	$(AVRDUDE) $(AVRDUDE_FLAGS) -U lfuse:w:0xe4:m -U hfuse:w:0xd8:m

# Clean target: remove all build artifacts and output files
clean:
	@echo "Cleaning build and output directories..."
	$(RM) -r $(BUILD_DIR) $(BIN_DIR)

# Rebuild target: clean and then build everything
rebuild: clean all

# Include automatically generated dependency files
# These files list the header dependencies for each .c file
# This ensures that if a header changes, only affected .c files are recompiled
-include $(DEPS)
