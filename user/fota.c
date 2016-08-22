#include "c_types.h"
#include "user_interface.h"
#include "espconn.h"
#include "mem.h"
#include "osapi.h"
#include "fota.h"
#include "user_json.h"

#define FOTA_PRIO              1
#define FOTA_QUEUE_SIZE        1
os_event_t fota_procTaskQueue[FOTA_QUEUE_SIZE];

#define HTTP_HEADER "Connection: close\r\n"\
                                        "Cache-Control: no-cache\r\n"\
                                        "User-Agent:device@tuanpm\r\n\r\n"

char path_download[128];
LOCAL int ICACHE_FLASH_ATTR
json_set(struct jsontree_context *js_ctx, struct jsonparse_state *parser)
{
  int type;
  while ((type = jsonparse_next(parser)) != 0) {
    if (type == JSON_TYPE_PAIR_NAME) {

      os_bzero(path_download, 128);
      if (jsonparse_strcmp_value(parser, "path") == 0) {
        jsonparse_next(parser);
        jsonparse_next(parser);
        jsonparse_copy_value(parser, path_download, sizeof(path_download));
        INFO("JSON: %s\r\n", path_download);
        return 0;
      }
    }
  }
}

LOCAL struct jsontree_callback path_callback =
  JSONTREE_CALLBACK(NULL, json_set);

JSONTREE_OBJECT(path_tree,
                JSONTREE_PAIR("path", &path_callback));

JSONTREE_OBJECT(data_tree,
                JSONTREE_PAIR("data", &path_tree));


static uint8 ICACHE_FLASH_ATTR
calc_chksum(uint8 *start, uint8 *end)
{
  uint8 chksum = CHKSUM_INIT;
  while (start < end) {
    chksum ^= *start;
    start++;
  }
  return chksum;
}




static void ICACHE_FLASH_ATTR
save_boot_cfg(espboot_cfg *cfg)
{
  cfg->chksum = calc_chksum((uint8*)cfg, (uint8*)&cfg->chksum);
  if (SPIEraseSector(BOOT_CONFIG_SECTOR) != 0)
  {
    INFO("Can not erase boot configuration sector\r\n");
  }
  if (SPIWrite(BOOT_CONFIG_SECTOR * SECTOR_SIZE, cfg, sizeof(espboot_cfg)) != 0)
  {
    INFO("Can not save boot configurations\r\n");
    while (1);
  }
}

static void ICACHE_FLASH_ATTR
load_boot_cfg(espboot_cfg *cfg)
{
  if (SPIRead(BOOT_CONFIG_SECTOR * SECTOR_SIZE, cfg, sizeof(espboot_cfg)) != 0)
  {
    INFO("Can not read boot configurations\r\n");
  }
  INFO("ESPBOOT: Load");
  if (cfg->magic != BOOT_CONFIG_MAGIC || cfg->chksum != calc_chksum((uint8*)cfg, (uint8*)&cfg->chksum))
  {
    INFO(" default");
    cfg->magic = BOOT_CONFIG_MAGIC;
    cfg->app_rom_addr = DEFAULT_APPROM_ADDR;
    cfg->backup_rom_addr = DEFAULT_BACKUPROM_ADDR;
    cfg->new_rom_addr = 0x000000; //not loader
    //save_boot_cfg(cfg);
  }
  INFO(" boot settings\r\n");
}

/**
  * @brief  Tcp client disconnect success callback function.
  * @param  arg: contain the ip link information
  * @retval None
  */
static void ICACHE_FLASH_ATTR
fota_tcpclient_discon_cb(void *arg)
{
  struct espconn *pConn = (struct espconn *)arg;
  fota_client* client = (fota_client *)pConn->reverse;

  INFO("[FOTA] Disconnect\r\n");
  client->conn_state = FOTA_CONN_DISCONNECT;

  system_os_post(FOTA_PRIO, 0, (os_param_t)client);
}

LOCAL void ICACHE_FLASH_ATTR
write(fota_client* client, uint8* data, uint32 len)
{
  uint32 begin_addr, len_write, current_sector, temp;
  uint8  *write_ptr = client->recv_buf, *read_ptr = data;

  if (len == 0) return;
  begin_addr = DEFAULT_NEWROM_ADDR + client->writen_len;
  while (len > 0)
  {
    len_write = 4096 - client->recv_len;
    if (len_write > len)
      len_write = len;
    write_ptr += client->recv_len;
    //INFO("[FOTA] Copy data to buffer: 0x%X, offset: %d, len: %d\r\n", write_ptr, client->recv_len, len_write);
    os_memcpy(write_ptr, read_ptr, len_write);
    write_ptr += len_write;
    client->recv_len += len_write;
    read_ptr += len_write;
    if (client->recv_len == 4096)
    {
      current_sector = begin_addr / SECTOR_SIZE;
      INFO(".");
      spi_flash_erase_sector(current_sector);
      SPIWrite(begin_addr, client->recv_buf, client->recv_len);

      client->writen_len += client->recv_len;
      begin_addr += client->recv_len;
      client->recv_len = 0;
      write_ptr = client->recv_buf;
    }

    len -= len_write;
  }

  if ((client->writen_len + client->recv_len) == client->content_len && client->recv_len != 0)
  {
    //final packet
    begin_addr = DEFAULT_NEWROM_ADDR + client->writen_len;
    current_sector = begin_addr / SECTOR_SIZE;
    INFO("..\r\n");
    spi_flash_erase_sector(current_sector);
    SPIWrite(begin_addr, client->recv_buf, client->recv_len);

    client->writen_len += client->recv_len;
    INFO("\r\nFOTA: Finish write %d bytes\r\n", client->writen_len);
    espboot_cfg cfg;
    load_boot_cfg(&cfg);
    cfg.new_rom_addr = DEFAULT_NEWROM_ADDR;
    save_boot_cfg(&cfg);
    INFO("\r\nFOTA: Finishing....\r\n");
    client->fota_state == FOTA_FINISHING;
    // os_delay_us(500000);
    //system_restart();
  }
}

/**
  * @brief  Udp server receive data callback function.
  * @param  arg: contain the ip link information
  * @retval None
  */
LOCAL void ICACHE_FLASH_ATTR
fota_tcpclient_recv(void *arg, char *pusrdata, unsigned short len)
{
  struct espconn *pConn = (struct espconn *)arg;
  fota_client* client = (fota_client *)pConn->reverse;
  uint32 pathLen = 0;
  char* ptr = 0, *binfile = 0, *ptrLen, *ptrData;

  if (client->fota_state == FOTA_SENDING_CHECKING)
  {
    //INFO("\r\nFOTA: Data received: %s\r\n", pusrdata);
    //Check received data
    ptr = (char*)os_strstr(pusrdata, "\r\n\r\n");
    binfile = (char*)os_strstr(pusrdata, ".bin");

    if (ptr != 0 && binfile != 0) {
      ptr += 4;
      pathLen = os_strlen(ptr);
      INFO("\r\nFOTA: Path: %d\r\n", pathLen);
      if (pathLen > 256) {
        client->fota_state = FOTA_IDLE;
        INFO("[FOTA] Path to long\r\n");
        return;
      }
      //if(client->path) os_free(client->path);

      //client->path = (uint8_t*)os_zalloc(pathLen + 1);
      //ptrData = (uint8_t*)os_zalloc(pathLen + 2);
      //os_strcpy(client->path, ptr);
      //os_strcpy(ptrData, ptr);
      //ptrData += pathLen - 1;
      //INFO("[FOTA] Path: %s\r\n", ptrData);
      struct jsontree_context js;

      jsontree_setup(&js, (struct jsontree_value *)&data_tree, json_putchar);
      json_parse(&js, ptr);

      INFO("[FOTA] Upgrade infomation received: %s\r\n", ptr);
      // if (client->path)
      //   os_free(client->path);
      // client->path = os_zalloc(128);

      // os_strcpy(client->path, path_download);
      client->path = path_download;
      //JSON_ParserPacket(ptr, pathLen);
      //client->fota_state = FOTA_GET_FILE;
      INFO("[FOTA] Download file: %s\r\n", client->path);
    } else {
      INFO("Invalid response\r\n");
      client->fota_state = FOTA_IDLE;
    }
  }
  else if (client->fota_state == FOTA_GETTING_FILE)
  {
    //INFO("INFO: Upgrade received length %d\r\n", client->recv_len);
    //First received data
    if (client->content_len == 0) {
      INFO("[FOTA] first packet len %d\r\n", len);
      if ((ptrLen = (char*)os_strstr(pusrdata, "Content-Length: "))
          && (ptrData = (char*)os_strstr(ptrLen, "\r\n\r\n"))
          && (os_strncmp(pusrdata + 9, "200", 3) == 0)) {

        // end of header/start of data
        ptrData += 4;
        // length of data after header in this chunk
        len -= (ptrData - pusrdata);

        ptrLen += 16;
        ptr = (char *)os_strstr(ptrLen, "\r\n");
        *ptr = '\0'; // destructive
        client->content_len = atoi(ptrLen);

        INFO("[FOTA] first packet %d, content len: %d\r\n", len, client->content_len);
        client->writen_len = 0;
        write(client, ptrData, len);
      }
    }
    else
    {
      //INFO("[FOTA] packet len %d\r\n", len);
      write(client, pusrdata, len);
    }
  }
  client->conn_state = FOTA_CONN_RECEIVED;
  system_os_post(FOTA_PRIO, 0, (os_param_t)client);
}

/******************************************************************************
 * FunctionName : user_esp_platform_sent_cb
 * Description  : Data has been sent successfully and acknowledged by the remote host.
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
fota_tcpclient_sent_cb(void *arg)
{
  struct espconn *pConn = (struct espconn *)arg;
  fota_client* client = (fota_client *)pConn->reverse;

  INFO("[FOTA] Data sent\r\n");
  client->conn_state = FOTA_CONN_SENT;
  system_os_post(FOTA_PRIO, 0, (os_param_t)client);
}

void ICACHE_FLASH_ATTR
fota_task(os_event_t *e)
{
//void ICACHE_FLASH_ATTR
//fota_task(fota_client *client)
//{
  uint8_t *temp;
  uint32_t sendLen;
  fota_client* client = (fota_client*)e->par;
  switch (client->conn_state) {

  case FOTA_CONN_CONNECTED:
    if (client->fota_state == FOTA_IDLE) {
      //send checking data
      INFO("[FOTA] Send request checking upgrade\r\n");

      temp = (uint8_t *)os_zalloc(512);
      sendLen = os_sprintf(temp, "GET /fota.json?dev=%s&ver=%s&token=%s&type=bin HTTP/1.1\r\nHost: %s:%d\r\n"HTTP_HEADER"",
                           client->device_id, client->version, client->access_key, client->host, client->port);
//
//                sendLen = os_sprintf(temp, "GET %s?dev=%08X&ver=%d&token=%s HTTP/1.1\r\nHost: %s:%d\r\n"HTTP_HEADER"",
//                                                                     client->path, client->device_id, client->ver, client->access_key, client->host, client->port);
      INFO("\r\n%s\r\n", temp);
      if (client->security)
        espconn_secure_sent(client->pCon, temp, sendLen);
      else
        espconn_sent(client->pCon, temp, sendLen);
      os_free(temp);

      client->fota_state = FOTA_SENDING_CHECKING;
      //client->fota_state = FOTA_GETTING_FILE;

    } else if (client->fota_state == FOTA_GET_FILE) {
      INFO("[FOTA] Send request bin file\r\n");

      temp = (uint8_t *)os_zalloc(512);
      sendLen = os_sprintf(temp, "GET %s HTTP/1.1\r\nHost: %s:%d\r\n"HTTP_HEADER"",
                           client->path, client->host, client->port);
      if (client->security)
        espconn_secure_sent(client->pCon, temp, sendLen);
      else
        espconn_sent(client->pCon, temp, sendLen);
      os_free(temp);
      client->fota_state = FOTA_GETTING_FILE;
      //send query get file
    }
    break;
  case FOTA_CONN_SENT:
    break;
  case FOTA_CONN_RECEIVED:

    if (client->fota_state == FOTA_FINISHING) {
      INFO("[FOTA] Disconnect from service\r\n");
      espconn_disconnect(client->pCon);
    }
    break;
  case FOTA_CONN_DISCONNECT:
    if (client->fota_state == FOTA_SENDING_CHECKING) {
      //connect to server get file
      INFO("[FOTA] Connect to FOTA server again to get bin file\r\n");
      if (client->security)
        espconn_secure_connect(client->pCon);
      else
        espconn_connect(client->pCon);
      client->fota_state = FOTA_GET_FILE;
    } else if (client->fota_state == FOTA_GETTING_FILE) {
      //cleanup connection
      //fota_disconnect(client);
      //client->fota_state = FOTA_FINISHING;
      INFO("[FOTA] Reboot to bootloader\r\n");
      system_restart();
      while(1);

    } else if (client->fota_state == FOTA_FINISHING) {
      INFO("[FOTA] Reboot to bootloader\r\n");
      system_restart();
      while(1);
    }
    break;

  case FOTA_CONN_INIT:
    //fota_connect(client);
    INFO("[FOTA] Setup fota server: %s:%d\r\n", client->host, client->port);
    break;
  }
  //TASK_Push(1, (os_param_t)client);
  //system_os_post(FOTA_PRIO, 0, (os_param_t)client);
}

void ICACHE_FLASH_ATTR
fota_init_client(fota_client *client, uint8_t* host, uint32 port, uint32 security, uint8_t* access_key, uint8_t* client_id, uint8_t* client_version)
{

  uint32_t temp;
  os_memset(client, 0, sizeof(fota_client));
  temp = os_strlen(host);
  client->host = (uint8_t*)os_zalloc(temp + 1);
  os_strcpy(client->host, host);
  client->host[temp] = 0;
  client->port = port;
  temp = os_strlen(client_id);
  client->device_id = (uint8_t*)os_zalloc(temp + 1);
  os_strcpy(client->device_id, client_id);
  client->device_id[temp] = 0;
  temp = os_strlen(access_key);
  client->access_key =  (uint8_t*)os_zalloc(temp + 1);;
  os_strcpy(client->access_key, access_key);
  client->access_key[temp] = 0;
  temp = os_strlen(client_version);
  client->version = (uint8_t*)os_zalloc(temp + 1);;
  os_strcpy(client->version, client_version);
  client->version[temp] = 0;

  client->conn_state = FOTA_CONN_INIT;
  client->fota_state = FOTA_IDLE;
  client->client_type = FOTA_CLIENT_TYPE;

  client->security = security;
  client->recv_buf = (uint8_t*)os_zalloc(4096);
  //INFO("[FOTA] Init client, 4096 bytes buffer: %d\r\n", client->recv_buf);
  client->recv_len = 0;
  //TASK_InitClient(1, client->clientType, fota_task);
  //TASK_Push(1, (os_param_t)client);
  system_os_task(fota_task, FOTA_PRIO, fota_procTaskQueue, FOTA_QUEUE_SIZE);
}


LOCAL void ICACHE_FLASH_ATTR
fota_dns_found(const char *name, ip_addr_t *ipaddr, void *arg)
{
  struct espconn *pConn = (struct espconn *)arg;
  fota_client* client = (fota_client *)pConn->reverse;


  if (ipaddr == NULL)
  {
    INFO("[FOTA] DNS found, but got no ip, try to reconnect\r\n");
    client->conn_state = FOTA_CONN_RECONNECT;
    return;
  }

  INFO("[FOTA] DNS found ip %d.%d.%d.%d\n",
       *((uint8 *) &ipaddr->addr),
       *((uint8 *) &ipaddr->addr + 1),
       *((uint8 *) &ipaddr->addr + 2),
       *((uint8 *) &ipaddr->addr + 3));

  if (client->ip.addr == 0 && ipaddr->addr != 0)
  {
    os_memcpy(client->pCon->proto.tcp->remote_ip, &ipaddr->addr, 4);

    if (client->security)
      espconn_secure_connect(client->pCon);
    else
      espconn_connect(client->pCon);


    client->conn_state = FOTA_CONN_CONNECTING;
    INFO("[FOTA] connecting...\r\n");
  }

  system_os_post(FOTA_PRIO, 0, (os_param_t)client);
}

/**
  * @brief  Tcp client connect success callback function.
  * @param  arg: contain the ip link information
  * @retval None
  */
void ICACHE_FLASH_ATTR
fota_tcpclient_connect_cb(void *arg)
{
  struct espconn *pCon = (struct espconn *)arg;
  fota_client* client = (fota_client *)pCon->reverse;
  char *temp = NULL;
  uint32_t sentLen;
  espconn_regist_disconcb(client->pCon, fota_tcpclient_discon_cb);
  espconn_regist_recvcb(client->pCon, fota_tcpclient_recv);////////
  espconn_regist_sentcb(client->pCon, fota_tcpclient_sent_cb);///////
  INFO("[FOTA] Connected to server %s:%d\r\n", client->host, client->port);
  client->conn_state = FOTA_CONN_CONNECTED;
  system_os_post(FOTA_PRIO, 0, (os_param_t)client);
}

/**
  * @brief  Tcp client connect repeat callback function.
  * @param  arg: contain the ip link information
  * @retval None
  */
void ICACHE_FLASH_ATTR
fota_tcpclient_recon_cb(void *arg, sint8 errType)
{
  struct espconn *pCon = (struct espconn *)arg;
  fota_client* client = (fota_client *)pCon->reverse;

  INFO("[FOTA] Reconnect to %s:%d\r\n", client->host, client->port);

  client->conn_state = FOTA_CONN_RECONNECT;

  system_os_post(FOTA_PRIO, 0, (os_param_t)client);
}

void ICACHE_FLASH_ATTR
fota_disconnect(fota_client *client)
{
  if (client->pCon) {
    INFO("[FOTA] Free memory\r\n");
    if (client->pCon->proto.tcp)
      os_free(client->pCon->proto.tcp);
    os_free(client->pCon);
    client->pCon = NULL;
    if (client->host)
      os_free(client->host);
    if (client->device_id)
      os_free(client->device_id);
    if (client->access_key)
      os_free(client->access_key);
    if (client->version)
      os_free(client->version);
    if (client->recv_buf)
      os_free(client->recv_buf);
  }
}

void ICACHE_FLASH_ATTR
fota_connect(fota_client *client)
{
  fota_disconnect(client);
  client->pCon = (struct espconn *)os_zalloc(sizeof(struct espconn));
  client->pCon->type = ESPCONN_TCP;
  client->pCon->state = ESPCONN_NONE;
  client->pCon->proto.tcp = (esp_tcp *)os_zalloc(sizeof(esp_tcp));
  client->pCon->proto.tcp->local_port = espconn_port();
  client->pCon->proto.tcp->remote_port = client->port;
  client->pCon->reverse = client;
  espconn_regist_connectcb(client->pCon, fota_tcpclient_connect_cb);
  espconn_regist_reconcb(client->pCon, fota_tcpclient_recon_cb);

  if (str_to_ip(client->host, &client->pCon->proto.tcp->remote_ip)) {
    INFO("[FOTA] Connect to ip  %s:%d\r\n", client->host, client->port);
    if (client->security)
      espconn_secure_connect(client->pCon);
    else
      espconn_connect(client->pCon);
  }
  else {
    INFO("[FOTA] Connect to domain %s:%d\r\n", client->host, client->port);
    espconn_gethostbyname(client->pCon, client->host, &client->ip, fota_dns_found);
  }
  client->conn_state = FOTA_CONN_CONNECTING;
  client->fota_state = FOTA_IDLE;
}

uint8_t ICACHE_FLASH_ATTR str_to_ip(const int8_t* str, void *ip)
{

  /* The count of the number of bytes processed. */
  int i;
  /* A pointer to the next digit to process. */
  const char * start;

  start = str;
  for (i = 0; i < 4; i++) {
    /* The digit being processed. */
    char c;
    /* The value of this byte. */
    int n = 0;
    while (1) {
      c = * start;
      start++;
      if (c >= '0' && c <= '9') {
        n *= 10;
        n += c - '0';
      }
      /* We insist on stopping at "." if we are still parsing
         the first, second, or third numbers. If we have reached
         the end of the numbers, we will allow any character. */
      else if ((i < 3 && c == '.') || i == 3) {
        break;
      }
      else {
        return 0;
      }
    }
    if (n >= 256) {
      return 0;
    }
    ((uint8_t*)ip)[i] = n;
  }
  return 1;

}
