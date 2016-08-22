#ifndef MODULES_INCLUDE_FOTA_H_
#define MODULES_INCLUDE_FOTA_H_

#define FOTA_CLIENT_TYPE 0xF01A
typedef enum {
    FOTA_CONN_INIT = 0x01,
    FOTA_CONN_RECONNECT,
    FOTA_CONN_CONNECTING,
    FOTA_CONN_SENDING,
    FOTA_CONN_CONNECTED,
    FOTA_CONN_SENT,
    FOTA_CONN_RECEIVED,
    FOTA_CONN_DISCONNECT,

    FOTA_SEND_CHECKING,
    FOTA_SENDING_CHECKING,
    FOTA_GET_FILE,
    FOTA_GETTING_FILE,
    FOTA_GOT_FILE,
    FOTA_CHECKING,
    FOTA_IDLE,
    FOTA_FINISHING

} tfota_state;

typedef struct  {
    uint32_t client_type;
    struct espconn *pCon;
    uint8_t* host;
    uint32_t port;
    uint8_t *path;
    ip_addr_t ip;
    uint32_t check_interval;
    uint8_t* access_key;
    uint8_t* device_id;
    uint8_t* version;
    tfota_state fota_state;
    tfota_state conn_state;
    void* user_data;

    uint32_t content_len;
    uint32_t writen_len;
    uint32_t security;
    uint8_t *recv_buf;
    uint32_t recv_len;
    uint32_t write_addr;
    uint8_t *recv_ptr;
    uint32_t ptr_len;
} fota_client;

typedef struct {
  uint8_t access_key[32]; //max 32 char
  uint8_t device_id[32]; //max 32
  uint8_t* version[8]; //max 8
  uint8_t host[32];
  uint32_t port;
  uint8_t path[128];
  uint32_t security;
} fota_info;
/* check interval
 * address: url or ip
 * port: http port is 80
 * getParams: /fota?rom=user1&ver=1&id=0xdevid&acessKey=key&format=json
 * Check Interval = 24*60*60 (second) or startup
 * Download link: GET /link?id=devid&accessKey=key
 */

typedef struct {
    uint32 magic;
    uint32 app_rom_addr;
    uint32 new_rom_addr;
    uint32 backup_rom_addr;
    uint8 chksum;
} espboot_cfg;

#define CHKSUM_INIT                     0xEF

#define SECTOR_SIZE                     0x1000
#define BOOT_CONFIG_SECTOR              1
#define BOOT_CONFIG_MAGIC               0xF01A
#define DEFAULT_APPROM_ADDR             0x2000
#define DEFAULT_NEWROM_ADDR             0x200000
#define DEFAULT_BACKUPROM_ADDR        0x300000

void fota_init_client(fota_client *client, uint8_t* host, uint32 port, uint32 security, uint8_t* access_key, uint8_t* client_id, uint8_t* client_version);
void fota_force_check();
void fota_disconnect(fota_client *client);
void fota_connect(fota_client *client);
uint8_t str_to_ip(const int8_t* str, void *ip);
//void FOTA_Task(fota_client *client);
#endif /* MODULES_INCLUDE_FOTA_H_ */
