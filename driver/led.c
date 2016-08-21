#include "driver/led.h"

#include "ets_sys.h"
#include "os_type.h"
#include "osapi.h"
#include "mem.h"
#include "gpio.h"
#include "user_interface.h"

static os_timer_t led_timer;
static uint32_t led_on_time = 0, led_off_time = 0, led_cnt = 0, led_is_on = 0;



void ICACHE_FLASH_ATTR
led_write(uint8_t value)
{
  WRITE_PERI_REG(RTC_GPIO_OUT, (READ_PERI_REG(RTC_GPIO_OUT) & (uint32_t)0xfffffffe) | (uint32_t)(value & 1));
}

void led_service_cb(void *args)
{
  if(led_cnt > 0) {
    led_cnt --;
  } else {
    if(led_is_on == 1 && led_off_time > 0) {
      led_is_on = 0;
      led_cnt = led_off_time;
    } else if(led_is_on == 0 && led_on_time > 0){
      led_is_on = 1;
      led_cnt = led_on_time;
    } else if(led_off_time == 0 && led_on_time == 0) {
      led_is_on = 0;
      led_cnt = 0;
    } else if(led_on_time == 0) {
      led_is_on = 0;
      led_cnt = led_off_time;
    } else if(led_off_time == 0) {
      led_is_on = 1;
      led_cnt = led_on_time;
    }
    led_write(led_is_on == 0);
  }
}

void ICACHE_FLASH_ATTR
led_init()
{
  WRITE_PERI_REG(PAD_XPD_DCDC_CONF, (READ_PERI_REG(PAD_XPD_DCDC_CONF) & 0xffffffbc) | (uint32_t)0x1);  // mux configuration for XPD_DCDC to output rtc_gpio0

  WRITE_PERI_REG(RTC_GPIO_CONF, (READ_PERI_REG(RTC_GPIO_CONF) & (uint32_t)0xfffffffe) | (uint32_t)0x0);  //mux configuration for out enable

  WRITE_PERI_REG(RTC_GPIO_ENABLE, (READ_PERI_REG(RTC_GPIO_ENABLE) & (uint32_t)0xfffffffe) | (uint32_t)0x1); //out enable
  os_timer_setfn(&led_timer, (os_timer_func_t *)led_service_cb, NULL);
  os_timer_arm(&led_timer, 100, 1);
  led_on_time = 10;
  led_off_time = 10; //on & off 1000ms
  led_is_on = 0;
}


void led_blink(uint32_t on_time, uint32_t off_time)
{
  led_on_time = on_time;
  led_off_time = off_time;
  led_cnt = 0;
  led_is_on = 1;
}

