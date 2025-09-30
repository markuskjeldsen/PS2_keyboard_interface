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
SRC_DIR      := src
INC_DIR      := include
BUILD_DIR    := build

# ---- Sources ----
SRC          := $(wildcard $(SRC_DIR)/*.c)
OBJ          := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRC))
DEP          := $(OBJ:.o=.d)

# ---- Flags ----
CSTD         := gnu11
CFLAGS := -mmcu=$(MCU) -DF_CPU=$(F_CPU) -DBAUD=$(BAUD) -std=$(CSTD) -Os \
          -Wall -Wextra -Wundef -Wstrict-prototypes -fdata-sections -ffunction-sections \
          -MMD -MP -I$(INC_DIR)
LDFLAGS      := -mmcu=$(MCU) -Wl,--gc-sections
AVRDUDE_FLAGS:= -p $(AVRDUDE_MCU) -c $(PROGRAMMER) $(AVRDUDE_SCK)

# ---- Targets ----
.PHONY: all flash fuses fuse-read clean size disasm

all: $(BUILD_DIR)/$(PROJECT).hex

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
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

