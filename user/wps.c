/*
* @Author: Tuan PM
* @Date:   2016-08-20 18:56:01
* @Last Modified by:   Tuan PM
* @Last Modified time: 2016-08-21 11:25:22
*/

#include "osapi.h"
#include "user_interface.h"
#include "driver/led.h"

LOCAL void ICACHE_FLASH_ATTR
wps_status_cb(int status)
{
  switch (status) {
    case WPS_CB_ST_SUCCESS:
      wifi_wps_disable();
      wifi_station_connect();
      led_blink(1000, 0);
      break;
    case WPS_CB_ST_FAILED:
    case WPS_CB_ST_TIMEOUT:
      wifi_wps_start();
      break;
  }
}


void wps_start()
{
  wifi_wps_disable();
  wifi_wps_enable(WPS_TYPE_PBC);
  wifi_set_wps_cb(wps_status_cb);
  wifi_wps_start();
}
