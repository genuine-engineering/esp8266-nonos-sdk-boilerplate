# ESP8266 NonOS Application sample

## Install

- Check install Compiler from: https://github.com/genuine-engineering/awesome-engineering/blob/master/esp8266/compiler.md 
- Download SDK 2.0.0 from: http://bbs.espressif.com/viewtopic.php?f=46&t=2451
- Download esptool.py from: https://github.com/themadinventor/esptool 

## Usage (Check detail from Makefile)

- OTA: Select from Makefile
```
OUTPUT_TYPE=combined/ota
```
- Base directory for the compiler. Needs a / at the end; if not set it'll use the tools that are in the PATH. Modify `XTENSA_TOOLS_ROOT`
- FLASH Selection:
```
#SPI flash size, in K
ESP_SPI_FLASH_SIZE_K=1024
#0: QIO, 1: QOUT, 2: DIO, 3: DOUT
ESP_FLASH_MODE=0
#0: 40MHz, 1: 26MHz, 2: 20MHz, 15: 80MHz
ESP_FLASH_FREQ_DIV=0
```

## Add module (check module folder user for sample)

```
MODULES   = folderA folderB
```

## Compile 
```
make  
make flash 
make clean
```
