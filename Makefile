MCU         := atmega16a
TARGET      := boiler
BUILD_DIR   := output
SRC_DIR     := src
BSP_DIR     := bsp
INC_DIR     := inc
OPT_FLAGS   ?= -Os
FAKE_SENSORS ?= 0
TEST_CONFIG  ?= 0
TEST_PROFILE ?= 2
FAKE_PRESENT ?= 3

CC          := avr-gcc
OBJCOPY     := avr-objcopy
SIZE        := avr-size
HOST_CC     ?= cc
PYTHON      ?= python3

CFLAGS      := -std=c11 -Wall -Wextra -Werror -mmcu=$(MCU) \
			   -I$(INC_DIR) -Ilib -I$(BSP_DIR) \
			   -include $(INC_DIR)/boiler_config.h $(OPT_FLAGS) \
			   -DSENSORS_USE_FAKE=$(FAKE_SENSORS) \
			   -DCONFIG_USE_TEST=$(TEST_CONFIG) \
			   -DCONFIG_TEST_PROFILE=$(TEST_PROFILE) \
			   -DSENSOR_FAKE_PRESENT_MASK=$(FAKE_PRESENT) \
			   -ffunction-sections -fdata-sections
LDFLAGS     := -mmcu=$(MCU) -Wl,-Map=$(BUILD_DIR)/$(TARGET).map \
			   -Wl,--gc-sections
LDLIBS      := -Llib -lacrosched

SOURCES     := $(SRC_DIR)/main.c $(SRC_DIR)/storage.c \
			   $(SRC_DIR)/config.c \
			   $(SRC_DIR)/display.c \
			   $(SRC_DIR)/feedback.c \
			   $(SRC_DIR)/buttons.c \
			   $(SRC_DIR)/sensors_fake.c \
			   $(BSP_DIR)/bsp.c
OBJECTS     := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SOURCES))
ELF         := $(BUILD_DIR)/$(TARGET).elf
HEX         := $(BUILD_DIR)/$(TARGET).hex

.PHONY: all clean debug size test

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

test:
	mkdir -p $(BUILD_DIR)/tests
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror -Itests/stubs -I$(INC_DIR) -Ilib \
		-I$(BSP_DIR) tests/display_test.c $(SRC_DIR)/display.c \
		$(SRC_DIR)/feedback.c \
		$(SRC_DIR)/buttons.c \
		$(SRC_DIR)/config.c \
		-o $(BUILD_DIR)/tests/display_test
	$(BUILD_DIR)/tests/display_test
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror -Itests/stubs -I$(INC_DIR) -Ilib \
		-I$(BSP_DIR) tests/storage_test.c $(SRC_DIR)/storage.c \
		-o $(BUILD_DIR)/tests/storage_test
	$(BUILD_DIR)/tests/storage_test
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror -Itests/stubs -I$(INC_DIR) -Ilib \
		-I$(BSP_DIR) tests/buttons_test.c $(SRC_DIR)/buttons.c \
		-o $(BUILD_DIR)/tests/buttons_test
	$(BUILD_DIR)/tests/buttons_test
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror -I$(INC_DIR) \
		-DSENSORS_USE_FAKE=1 tests/sensors_test.c $(SRC_DIR)/sensors_fake.c \
		-o $(BUILD_DIR)/tests/sensors_test
	$(BUILD_DIR)/tests/sensors_test
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror -I$(INC_DIR) -DSENSORS_USE_FAKE=1 \
		tests/config_test.c $(SRC_DIR)/config.c $(SRC_DIR)/sensors_fake.c \
		-o $(BUILD_DIR)/tests/config_test
	$(BUILD_DIR)/tests/config_test
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror -I$(INC_DIR) \
		-DSENSORS_USE_FAKE=1 -DCONFIG_USE_TEST=1 \
		tests/config_test.c $(SRC_DIR)/config.c $(SRC_DIR)/sensors_fake.c \
		-o $(BUILD_DIR)/tests/config_ram_test
	$(BUILD_DIR)/tests/config_ram_test
	for profile in 1 2; do \
		for mask in 0 1 2 3; do \
			$(HOST_CC) -std=c11 -Wall -Wextra -Werror \
				-Itests/stubs -I$(INC_DIR) -Ilib -I$(BSP_DIR) \
				-DSENSORS_USE_FAKE=1 -DCONFIG_USE_TEST=1 \
				-DCONFIG_TEST_PROFILE=$$profile \
				-DSENSOR_FAKE_PRESENT_MASK=$$mask \
				tests/config_startup_test.c $(SRC_DIR)/config.c \
				$(SRC_DIR)/sensors_fake.c $(SRC_DIR)/display.c \
				$(SRC_DIR)/feedback.c $(SRC_DIR)/buttons.c \
				-o $(BUILD_DIR)/tests/config_startup_test && \
				$(BUILD_DIR)/tests/config_startup_test || exit 1; \
		done; \
	done
	$(PYTHON) -m unittest discover -s tests -p '*_test.py'
