MCU         := atmega16a
F_CPU       := 14745600UL
TARGET      := boiler
BUILD_DIR   := output
SRC_DIR     := src

CC          := avr-gcc
OBJCOPY     := avr-objcopy
SIZE        := avr-size

CFLAGS      := -std=c11 -Wall -Wextra -Werror -mmcu=$(MCU) \
			   -DF_CPU=$(F_CPU) -Ilib -Os
LDFLAGS     := -mmcu=$(MCU) -Wl,-Map=$(BUILD_DIR)/$(TARGET).map \
			   -Llib -lacrosched

SOURCE      := $(SRC_DIR)/main.c
OBJECT      := $(BUILD_DIR)/main.o
ELF         := $(BUILD_DIR)/$(TARGET).elf
HEX         := $(BUILD_DIR)/$(TARGET).hex

.PHONY: all clean size

all: $(HEX)

$(HEX): $(ELF)
	$(OBJCOPY) -O ihex -R .eeprom $< $@

$(ELF): $(OBJECT) | $(BUILD_DIR)
	$(CC) $(LDFLAGS) -o $@ $^

$(OBJECT): $(SOURCE) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $@

size: $(ELF)
	$(SIZE) --format=avr --mcu=$(MCU) $(ELF)

clean:
	rm -rf $(BUILD_DIR)