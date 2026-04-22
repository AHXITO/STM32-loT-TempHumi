#ifndef __ESP8266_H
#define __ESP8266_H

void ESP8266_Init(void);
uint8_t ESP8266_SendCmd(char *cmd,char *ack,uint16_t timeout);
uint8_t ESP8266_WaitAck(char *ack);
uint8_t ESP8266_ConnectWiFi(char*ssid,char *pwd);
uint8_t ESP8266_IsWiFiConnected(void);
uint8_t ESP8266_MaintainWiFi(uint8_t *wifi_ok);
void ESP8266_Close(void);
void ESP8266_SendData(char *data);
uint8_t ESP8266_StartTCP(char *ip, int port);
uint8_t ESP8266_MqttPubRaw(const char *topic, const char *payload, uint16_t wait_prompt_ms, uint16_t wait_ok_ms);

#endif
