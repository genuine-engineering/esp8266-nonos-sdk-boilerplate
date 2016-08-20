#include "driver/led.h"

#include "ets_sys.h"
#include "os_type.h"
#include "osapi.h"
#include "mem.h"
#include "gpio.h"
#include "user_interface.h"

os_timer_t led_timer;

void led_service_cb()
{
  static uint8_t led_val = 0;
  led_set(led_val);
  if (led_val)
    led_val = 0;
  else
    led_val = 1;
}

void ICACHE_FLASH_ATTR
led_set(uint8_t value)
{
  WRITE_PERI_REG(RTC_GPIO_OUT, (READ_PERI_REG(RTC_GPIO_OUT) & (uint32_t)0xfffffffe) | (uint32_t)(value & 1));
}

void ICACHE_FLASH_ATTR
led_init()
{
  WRITE_PERI_REG(PAD_XPD_DCDC_CONF, (READ_PERI_REG(PAD_XPD_DCDC_CONF) & 0xffffffbc) | (uint32_t)0x1);  // mux configuration for XPD_DCDC to output rtc_gpio0

  WRITE_PERI_REG(RTC_GPIO_CONF, (READ_PERI_REG(RTC_GPIO_CONF) & (uint32_t)0xfffffffe) | (uint32_t)0x0);  //mux configuration for out enable

  WRITE_PERI_REG(RTC_GPIO_ENABLE, (READ_PERI_REG(RTC_GPIO_ENABLE) & (uint32_t)0xfffffffe) | (uint32_t)0x1); //out enable
  os_timer_setfn(&led_timer, (os_timer_func_t *)led_service_cb, NULL);
  os_timer_arm(&led_timer, 500, 1);
}


void led_blink()
{

}

