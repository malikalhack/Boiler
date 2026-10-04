MCU         := atmega16a
TARGET      := boiler
BUILD_DIR   := output
SRC_DIR     := src
BSP_DIR     := bsp
INC_DIR     := inc
OPT_FLAGS   ?= -Os

CC          := avr-gcc
OBJCOPY     := avr-objcopy
SIZE        := avr-size

CFLAGS      := -std=c11 -Wall -Wextra -Werror -mmcu=$(MCU) \
			   -I$(INC_DIR) -Ilib -I$(BSP_DIR) \
			   -include $(INC_DIR)/boiler_config.h $(OPT_FLAGS) \
			   -ffunction-sections -fdata-sections
LDFLAGS     := -mmcu=$(MCU) -Wl,-Map=$(BUILD_DIR)/$(TARGET).map \
			   -Wl,--gc-sections
LDLIBS      := -Llib -lacrosched

SOURCES     := $(SRC_DIR)/main.c $(SRC_DIR)/boiler_storage.c \
			   $(BSP_DIR)/bsp.c
OBJECTS     := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SOURCES))
ELF         := $(BUILD_DIR)/$(TARGET).elf
HEX         := $(BUILD_DIR)/$(TARGET).hex

.PHONY: all clean debug size

all: $(HEX) size

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
