/******************************************************************************
 * Copyright 2013-2014 Espressif Systems (Wuxi)
 *
 * FileName: user_main.c
 *
 * Description: entry file of user application
 *
 * Modification history:
 *     2015/7/3, v1.0 create this file.
*******************************************************************************/

#include "osapi.h"
#include "user_interface.h"

#include "driver/key.h"
#include "driver/uart.h"
#include "driver/led.h"
#include "wps.h"
#include "sc.h"

#define KEY_NUM        1

#define KEY_IO_MUX     PERIPHS_IO_MUX_MTCK_U
#define KEY_IO_NUM     0
#define KEY_IO_FUNC    FUNC_GPIO0



LOCAL struct keys_param keys;
LOCAL struct single_key_param *single_key;

LOCAL void ICACHE_FLASH_ATTR
short_press(void)
{
  INFO("[KEY] Short press, run wps\r\n");
  wps_start();
}

LOCAL void ICACHE_FLASH_ATTR
long_press(void)
{
  INFO("[KEY] Long press, run smartconfig\r\n");
  sc_start();
}


void ICACHE_FLASH_ATTR print_info()
{
  INFO("BOOTUP\r\n");
  INFO("[INFO] SDK: %s\r\n", system_get_sdk_version());
  INFO("[INFO] Chip ID: %08X\r\n", system_get_chip_id());
  INFO("[INFO] Memory info:\r\n");
  system_print_meminfo();
}


void ICACHE_FLASH_ATTR app_init()
{
  uart_init(BIT_RATE_115200, BIT_RATE_115200);

  print_info();

  single_key = key_init_single(KEY_IO_NUM, KEY_IO_MUX, KEY_IO_FUNC,
                                        long_press, short_press);

  keys.key_num = KEY_NUM;
  keys.single_key = &single_key;

  key_init(&keys);
  led_init();

  wifi_set_opmode_current(STATION_MODE);

}

void ICACHE_FLASH_ATTR user_init(void)
{
  system_init_done_cb(app_init);

}
