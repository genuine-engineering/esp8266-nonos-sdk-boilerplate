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
//#include "task.h"
#include "fota.h"

#define KEY_NUM        1

#define KEY_IO_MUX     PERIPHS_IO_MUX_MTCK_U
#define KEY_IO_NUM     0
#define KEY_IO_FUNC    FUNC_GPIO0


LOCAL fota_client client;

LOCAL struct keys_param keys;
LOCAL struct single_key_param *single_key;

LOCAL void ICACHE_FLASH_ATTR
short_press(void)
{
  INFO("[KEY] Short press, run smartconfig\r\n");
  led_blink(1, 1);
  sc_start();
  //fota_connect(&client);
}

LOCAL void ICACHE_FLASH_ATTR
long_press(void)
{
  INFO("[KEY] Long press, run wps\r\n");
  led_blink(5, 5);
  //wps_start();
  fota_connect(&client);
}


void ICACHE_FLASH_ATTR print_info()
{
  INFO("\r\n\r\n[INFO] BOOTUP...\r\n");
  INFO("[INFO] SDK: %s\r\n", system_get_sdk_version());
  INFO("[INFO] Chip ID: %08X\r\n", system_get_chip_id());
  INFO("[INFO] Memory info:\r\n");
  system_print_meminfo();

  INFO("[INFO] -------------------------------------------\n");
  INFO("[INFO] Build time: %s\n", BUID_TIME);
  INFO("[INFO] -------------------------------------------\n");

}


void ICACHE_FLASH_ATTR app_init()
{
  // const fota_info fota_conenction = {
  //   .host = "test.vidieukhien.net",
  //   .port = "80",
  //   .security = 0,
  //   .device_id = "device_id",
  //   .access_key = "access_key",
  //   .version = "version",
  //   .path = "/fota.json?dev={device_id|%X}&token={access_key|%s}&version={version:%s}"
  // };

  uart_init(BIT_RATE_115200, BIT_RATE_115200);

  print_info();

  single_key = key_init_single(KEY_IO_NUM, KEY_IO_MUX, KEY_IO_FUNC,
                                        long_press, short_press);

  keys.key_num = KEY_NUM;
  keys.single_key = &single_key;

  key_init(&keys);
  led_init();
  led_blink(10, 10); //1 second on, 1 second off

  fota_init_client(&client,
                    "test.vidieukhien.net",
                    80,
                    0,
                    "access_key",
                    "chipid",
                    "version");

  wifi_set_opmode_current(STATION_MODE);

}

void ICACHE_FLASH_ATTR user_init(void)
{
  system_init_done_cb(app_init);

}
