#include "ets_sys.h"
#include "osapi.h"
#include "ip_addr.h"
#include "espconn.h"
#include "mem.h"


#include "user_interface.h"
#include "smartconfig.h"
#include "airkiss.h"

#include "driver/led.h"


#define DEVICE_TYPE     "gh_9e2cff3dfa51" //wechat public number
#define DEVICE_ID       "122475" //model ID

#define DEFAULT_LAN_PORT  12476
uint32_t sc_run = 0;
LOCAL esp_udp ssdp_udp;
LOCAL struct espconn pssdpudpconn;
LOCAL os_timer_t ssdp_time_serv;

uint8_t  lan_buf[200];
uint16_t lan_buf_len;
uint8    udp_sent_cnt = 0;

const airkiss_config_t akconf =
{
  (airkiss_memset_fn)&memset,
  (airkiss_memcpy_fn)&memcpy,
  (airkiss_memcmp_fn)&memcmp,
  0,
};

LOCAL void ICACHE_FLASH_ATTR
airkiss_wifilan_time_callback(void)
{
  uint16 i;
  airkiss_lan_ret_t ret;

  if ((udp_sent_cnt++) > 30) {
    udp_sent_cnt = 0;
    os_timer_disarm(&ssdp_time_serv);//s
    //return;
  }

  ssdp_udp.remote_port = DEFAULT_LAN_PORT;
  ssdp_udp.remote_ip[0] = 255;
  ssdp_udp.remote_ip[1] = 255;
  ssdp_udp.remote_ip[2] = 255;
  ssdp_udp.remote_ip[3] = 255;
  lan_buf_len = sizeof(lan_buf);
  ret = airkiss_lan_pack(AIRKISS_LAN_SSDP_NOTIFY_CMD,
                         DEVICE_TYPE, DEVICE_ID, 0, 0, lan_buf, &lan_buf_len, &akconf);
  if (ret != AIRKISS_LAN_PAKE_READY) {
    INFO("[SC] Pack lan packet error!");
    return;
  }

  ret = espconn_sendto(&pssdpudpconn, lan_buf, lan_buf_len);
  if (ret != 0) {
    INFO("[SC] UDP send error!");
  }
  INFO("[SC] Finish send notify!\n");
}

LOCAL void ICACHE_FLASH_ATTR
airkiss_wifilan_recv_callbk(void *arg, char *pdata, unsigned short len)
{
  uint16 i;
  remot_info* pcon_info = NULL;

  airkiss_lan_ret_t ret = airkiss_lan_recv(pdata, len, &akconf);
  airkiss_lan_ret_t packret;

  switch (ret) {
  case AIRKISS_LAN_SSDP_REQ:
    espconn_get_connection_info(&pssdpudpconn, &pcon_info, 0);
    INFO("[SC] remote ip: %d.%d.%d.%d \r\n", pcon_info->remote_ip[0], pcon_info->remote_ip[1],
              pcon_info->remote_ip[2], pcon_info->remote_ip[3]);
    INFO("[SC] remote port: %d \r\n", pcon_info->remote_port);

    pssdpudpconn.proto.udp->remote_port = pcon_info->remote_port;
    os_memcpy(pssdpudpconn.proto.udp->remote_ip, pcon_info->remote_ip, 4);
    ssdp_udp.remote_port = DEFAULT_LAN_PORT;

    lan_buf_len = sizeof(lan_buf);
    packret = airkiss_lan_pack(AIRKISS_LAN_SSDP_RESP_CMD,
                               DEVICE_TYPE, DEVICE_ID, 0, 0, lan_buf, &lan_buf_len, &akconf);

    if (packret != AIRKISS_LAN_PAKE_READY) {
      INFO("[SC] Pack lan packet error!");
      return;
    }

    INFO("\r\n\r\n");
    for (i = 0; i < lan_buf_len; i++)
      INFO("%c", lan_buf[i]);
    INFO("\r\n\r\n");

    packret = espconn_sendto(&pssdpudpconn, lan_buf, lan_buf_len);
    if (packret != 0) {
      INFO("[SC] LAN UDP Send err!");
    }
    os_timer_disarm(&ssdp_time_serv);//s
    break;
  default:
    INFO("[SC] Pack is not ssdq req!%d\r\n", ret);
    break;
  }
}

void ICACHE_FLASH_ATTR
airkiss_start_discover(void)
{
  ssdp_udp.local_port = DEFAULT_LAN_PORT;
  pssdpudpconn.type = ESPCONN_UDP;
  pssdpudpconn.proto.udp = &(ssdp_udp);
  espconn_regist_recvcb(&pssdpudpconn, airkiss_wifilan_recv_callbk);
  espconn_create(&pssdpudpconn);

  os_timer_disarm(&ssdp_time_serv);
  os_timer_setfn(&ssdp_time_serv, (os_timer_func_t *)airkiss_wifilan_time_callback, NULL);
  os_timer_arm(&ssdp_time_serv, 1000, 1);//1s
}


void ICACHE_FLASH_ATTR
smartconfig_done(sc_status status, void *pdata)
{
  switch (status) {
  case SC_STATUS_WAIT:
    INFO("[SC] SC_STATUS_WAIT\n");
    break;
  case SC_STATUS_FIND_CHANNEL:
    INFO("[SC] SC_STATUS_FIND_CHANNEL\n");
    break;
  case SC_STATUS_GETTING_SSID_PSWD:
    INFO("[SC] SC_STATUS_GETTING_SSID_PSWD\n");
    sc_type *type = pdata;
    if (*type == SC_TYPE_ESPTOUCH) {
      INFO("[SC] SC_TYPE:SC_TYPE_ESPTOUCH\n");
    } else {
      INFO("[SC] SC_TYPE:SC_TYPE_AIRKISS\n");
    }
    break;
  case SC_STATUS_LINK:
    INFO("[SC] SC_STATUS_LINK\n");
    struct station_config *sta_conf = pdata;

    wifi_station_set_config(sta_conf);
    wifi_station_disconnect();
    wifi_station_connect();
    led_blink(1, 0);
    break;
  case SC_STATUS_LINK_OVER:
    INFO("[SC] SC_STATUS_LINK_OVER\n");
    if (pdata != NULL) {
      //SC_TYPE_ESPTOUCH
      uint8 phone_ip[4] = {0};

      os_memcpy(phone_ip, (uint8*)pdata, 4);
      INFO("[SC] Phone ip: %d.%d.%d.%d\n", phone_ip[0], phone_ip[1], phone_ip[2], phone_ip[3]);
    } else {
      //SC_TYPE_AIRKISS - support airkiss v2.0
      airkiss_start_discover();
    }
    smartconfig_stop();
    sc_run = 0;
    break;
  }

}



void ICACHE_FLASH_ATTR
sc_start(void)
{
  smartconfig_set_type(SC_TYPE_ESPTOUCH_AIRKISS); //SC_TYPE_ESPTOUCH,SC_TYPE_AIRKISS,SC_TYPE_ESPTOUCH_AIRKISS
  if(sc_run)
    smartconfig_stop();
  smartconfig_start(smartconfig_done);
  sc_run = 1;
  INFO("[SC] Started\r\n");
}
