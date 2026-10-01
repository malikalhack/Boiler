MCU         := atmega16a
F_CPU       := 14745600UL
TARGET      := boiler
BUILD_DIR   := output
SRC_DIR     := src
BSP_DIR     := bsp
OPT_FLAGS   ?= -Os

CC          := avr-gcc
OBJCOPY     := avr-objcopy
SIZE        := avr-size

CFLAGS      := -std=c11 -Wall -Wextra -Werror -mmcu=$(MCU) \
               -DF_CPU=$(F_CPU) -Ilib -I$(BSP_DIR) $(OPT_FLAGS)
LDFLAGS     := -mmcu=$(MCU) -Wl,-Map=$(BUILD_DIR)/$(TARGET).map
LDLIBS      := -Llib -lacrosched

SOURCES     := $(SRC_DIR)/main.c $(BSP_DIR)/bsp.c
OBJECTS     := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SOURCES))
ELF         := $(BUILD_DIR)/$(TARGET).elf
HEX         := $(BUILD_DIR)/$(TARGET).hex

.PHONY: all clean debug size

all: $(HEX)

$(HEX): $(ELF)
	$(OBJCOPY) -O ihex -R .eeprom $< $@


$(ELF): $(OBJECTS) | $(BUILD_DIR)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(BUILD_DIR)/%.o: %.c
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $@

size: $(ELF)
	$(SIZE) --format=avr --mcu=$(MCU) $(ELF)

clean:
	rm -rf $(BUILD_DIR)

debug: clean
	$(MAKE) OPT_FLAGS="-Og -g3" all size
