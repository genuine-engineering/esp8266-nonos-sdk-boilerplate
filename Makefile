# none sdkota espboot rboot
OTA ?= espboot
OTA_APP_ADDR = 0x2000
OTA_BOOTLOADER_PATH = ../esp-bootloader/firmware/espboot.bin

# Base directory for the compiler. Needs a / at the end; if not set it'll use the tools that are in
# the PATH.
XTENSA_TOOLS_ROOT ?=

# base directory of the ESP8266 SDK package, absolute
SDK_BASE	?= /tools/esp8266/sdk/ESP8266_NONOS_SDK

#Esptool.py path and port
ESPTOOL		?= /tools/esp8266/esptool/esptool.py
ESPPORT		?= /dev/tty.SLAB_USBtoUART
#ESPDELAY indicates seconds to wait between flashing the two binary images
ESPDELAY	?= 3
ESPBAUD		?= 460800

# 40m 26m 20m 80m
ESP_FREQ = 40m
# qio qout dio dout
ESP_MODE = qio
#4m 2m 8m 16m 32m
ESP_SIZE = 32m


VERBOSE = no
FLAVOR = debug
# name for the target project
TARGET		?= esp8266-nonos-app

# which modules (subdirectories) of the project to include in compiling
USER_MODULES		= user driver
USER_INC				= include
USER_LIB				=


SDK_LIBDIR = lib
SDK_LIBS = c gcc hal phy pp net80211 wpa main lwip crypto wps airkiss smartconfig ssl
SDK_INC = include include/json



# Output directors to store intermediate compiled files
# relative to the project directory
BUILD_BASE				= build
FIRMWARE_BASE		= firmware

# Opensdk patches stdint.h when compiled with an internal SDK. If you run into compile problems pertaining to
# redefinition of int types, try setting this to 'yes'.
USE_OPENSDK ?= no

DATETIME := $(shell date "+%Y-%b-%d_%H:%M:%S_%Z")

# select which tools to use as compiler, librarian and linker
CC		:= $(XTENSA_TOOLS_ROOT)xtensa-lx106-elf-gcc
AR		:= $(XTENSA_TOOLS_ROOT)xtensa-lx106-elf-ar
LD		:= $(XTENSA_TOOLS_ROOT)xtensa-lx106-elf-gcc
OBJCOPY	:= $(XTENSA_TOOLS_ROOT)xtensa-lx106-elf-objcopy



####
#### no user configurable options below here
####
SRC_DIR				:= $(USER_MODULES)
BUILD_DIR			:= $(addprefix $(BUILD_BASE)/,$(USER_MODULES))

INCDIR	:= $(addprefix -I,$(SRC_DIR))
EXTRA_INCDIR	:= $(addprefix -I,$(USER_INC))
MODULE_INCDIR	:= $(addsuffix /include,$(INCDIR))

SDK_LIBDIR	:= $(addprefix $(SDK_BASE)/,$(SDK_LIBDIR))
SDK_LIBS 		:= $(addprefix -l,$(SDK_LIBS))

SDK_INCDIR	:= $(addprefix -I$(SDK_BASE)/,$(SDK_INC))

SRC		:= $(foreach sdir,$(SRC_DIR),$(wildcard $(sdir)/*.c))
ASMSRC		= $(foreach sdir,$(SRC_DIR),$(wildcard $(sdir)/*.S))

OBJ		= $(patsubst %.c,$(BUILD_BASE)/%.o,$(SRC))
OBJ		+= $(patsubst %.S,$(BUILD_BASE)/%.o,$(ASMSRC))

APP_AR		:= $(addprefix $(BUILD_BASE)/,$(TARGET).a)
TARGET_OUT	:= $(addprefix $(BUILD_BASE)/,$(TARGET).out)



# compiler flags using during compilation of source files
CFLAGS		= -g			\
						-Wpointer-arith		\
						-Wundef			\
						-Wl,-EL			\
						-fno-inline-functions	\
						-nostdlib       \
						-mlongcalls	\
						-mtext-section-literals \
						-ffunction-sections \
						-fdata-sections	\
						-fno-builtin-printf\
						-DICACHE_FLASH \
						-DBUID_TIME=\"$(DATETIME)\"

# linker flags used to generate the main object file
LDFLAGS		= -nostdlib -Wl,--no-check-sections -u call_user_start -Wl,-static

ifeq ($(FLAVOR),debug)
    LDFLAGS += -g -O2
endif

ifeq ($(FLAVOR),release)
    LDFLAGS += -g -O0
endif

V ?= $(VERBOSE)
ifeq ("$(V)","yes")
Q :=
vecho := @true
else
Q := @
vecho := @echo
endif

ifeq ("$(USE_OPENSDK)","yes")
CFLAGS		+= -DUSE_OPENSDK
else
CFLAGS		+= -D_STDINT_H
endif


define maplookup
	$(patsubst $(strip $(1)):%,%,$(filter $(strip $(1)):%,$(2)))
endef



ESPTOOL_OPTS=--port $(ESPPORT) --baud $(ESPBAUD)


ifeq ("$(OTA)","espboot")
	OUTPUT := $(addprefix $(FIRMWARE_BASE)/,$(TARGET)-0x2000.bin)
	ESPTOOL_WRITE = write_flash 0x0 $(OTA_BOOTLOADER_PATH) $(OTA_APP_ADDR) $(OUTPUT) -fs 32m
	ESPTOOL_FLASHDEF=--flash_freq $(ESP_FREQ) --flash_mode $(ESP_MODE) --flash_size $(ESP_SIZE) --version=2
	LD_SCRIPT	= -Tld/with-espboot-flash-at-0x2000-size-1M.ld
else
	OUTPUT := $(addprefix $(FIRMWARE_BASE)/,$(TARGET))
	ESPTOOL_WRITE = write_flash 0x00000 $(OUTPUT)0x00000.bin 0x10000 $(OUTPUT)0x10000.bin -fs $(ESP_SIZE)
	ESPTOOL_FLASHDEF=
	LD_SCRIPT	= -Tld/without-bootloader.ld
endif


vpath %.c $(SRC_DIR)

define compile-objects
$1/%.o: %.c
	$(vecho) "CC $$<"
	$(Q) $(CC) $(INCDIR) $(MODULE_INCDIR) $(EXTRA_INCDIR) $(SDK_INCDIR) $(CFLAGS)  -c $$< -o $$@
endef



.PHONY: all checkdirs clean

all: touch checkdirs $(OUTPUT)

touch:
	$(vecho) "BUID TIME $(DATETIME)"
	$(Q) touch user/user_main.c

checkdirs: $(BUILD_DIR) $(FIRMWARE_BASE)

$(OUTPUT): $(TARGET_OUT)
	$(vecho) "FW $@"
	$(Q) $(ESPTOOL) elf2image $(ESPTOOL_FLASHDEF) $< -o $(OUTPUT)

$(BUILD_DIR):
	$(Q) mkdir -p $@

$(FIRMWARE_BASE):
	$(Q) mkdir -p $@

$(TARGET_OUT): $(APP_AR)
	$(vecho) "LD $@"
	$(Q) $(LD) -L$(SDK_LIBDIR) $(LD_SCRIPT) $(LDFLAGS) -Wl,--start-group $(SDK_LIBS) $(APP_AR) -Wl,--end-group -o $@

$(APP_AR): $(OBJ)
	$(vecho) "AR $@"
	$(Q) $(AR) cru $@ $^

flash:
	$(ESPTOOL) $(ESPTOOL_OPTS) $(ESPTOOL_WRITE)

f: clean all flash openport

upload:
	scp $(OUTPUT) root@vidieukhien.net:/var/www/test/

openport:
	$(vecho) "After flash, terminal will enter serial port screen"
	$(vecho) "Please exit with command:"
	$(vecho) "\033[0;31m" "Ctrl + A + k" "\033[0m"

	#@read -p "Press any key to continue... " -n1 -s
	@screen $(ESPPORT) 115200

clean:
	$(Q) rm -rf $(BUILD_DIR)
	$(Q) rm -rf $(FIRMWARE_BASE)
$(foreach bdir,$(BUILD_DIR),$(eval $(call compile-objects,$(bdir))))
