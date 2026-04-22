#include "stm32f10x.h"                  // Device header
#include "usart2.h"
#include <string.h>
#include "Delay.h"
#include "esp8266.h"
#include <stdio.h>

#define WIFI_SSID  "512"
#define WIFI_PWD   "12345678"

uint8_t ESP8266_SendCmd(char *cmd,char *ack,uint16_t timeout)
{
	ESP_ClearBuffer();
	USART2_SendString(cmd);
	while(timeout--)
	{
		if(strstr(esp_rx_buffer,ack)!=NULL)
		{
			return 1;
		}
		if(strstr(esp_rx_buffer, "ERROR") != NULL)
        {
            return 0;
        }
		Delay_ms(1);
	}
	return 0;
}

uint8_t ESP8266_ConnectWiFi(char*ssid,char *pwd)
{
	char cmd[120];

    sprintf(cmd, "AT+CWJAP=\"%s\",\"%s\"\r\n", ssid, pwd);

    return ESP8266_SendCmd(cmd, "OK", 8000);
}

//查询是否连接
uint8_t ESP8266_IsWiFiConnected(void)
{
    
    if (!ESP8266_SendCmd("AT+CWJAP?\r\n", "OK", 1500)) {
        return 0;
    }
    if (strstr(esp_rx_buffer, "+CWJAP:") != NULL) {
        return 1;
    }
    return 0;
}

//保活，断线重连
uint8_t ESP8266_MaintainWiFi(uint8_t *wifi_ok)
{
    uint8_t ok;

    ok = ESP8266_IsWiFiConnected();
    if (wifi_ok != NULL) {
        *wifi_ok = ok;
    }
    if (ok) {
        return 1;
    }

    /* 断线自动重连*/
    (void)ESP8266_SendCmd("AT+CWMODE=1\r\n", "OK", 500);
    if (!ESP8266_ConnectWiFi(WIFI_SSID, WIFI_PWD)) {
        if (wifi_ok != NULL) {
            *wifi_ok = 0;
        }
        return 0;
    }

    if (wifi_ok != NULL) {
        *wifi_ok = 1;
    }
    return 1;
}

void ESP8266_Init(void)
{

    ESP8266_SendCmd("AT+RST\r\n", "OK", 1500);
    Delay_ms(2000);

    while (!ESP8266_SendCmd("AT\r\n", "OK", 500))
    {
        Delay_ms(500);
    }

    while (!ESP8266_SendCmd("ATE0\r\n", "OK", 200))
    {
        Delay_ms(200);
    }

    while (!ESP8266_SendCmd("AT+CWMODE=1\r\n", "OK", 500))
    {
        Delay_ms(200);
    }

    while (!ESP8266_ConnectWiFi(WIFI_SSID, WIFI_PWD))
    {
        Delay_ms(2000);
    }
}




uint8_t ESP8266_StartTCP(char *ip, int port)
{
    char cmd[100];

    sprintf(cmd, "AT+CIPSTART=\"TCP\",\"%s\",%d\r\n", ip, port);

    return ESP8266_SendCmd(cmd, "OK", 3000);
}

void ESP8266_SendData(char *data)
{
    char cmd[50];

    sprintf(cmd, "AT+CIPSEND=%d\r\n", strlen(data));

    if(ESP8266_SendCmd(cmd, ">", 200))
    {
        USART2_SendString(data);
    }
}

void ESP8266_Close(void)
{
    ESP8266_SendCmd("AT+CIPCLOSE\r\n", "OK", 200);
}

//把指定的 topic 和（内容）发布到 MQTT 服务器
uint8_t ESP8266_MqttPubRaw(const char *topic, const char *payload, uint16_t wait_prompt_ms, uint16_t wait_ok_ms)
{
    char cmd[256];
    size_t len;
    uint16_t t;

    if (topic == NULL || payload == NULL) {
        return 0;
    }
    len = strlen(payload);
    if (len == 0u || len > 2000u) {
        return 0;
    }

    sprintf(cmd, "AT+MQTTPUBRAW=0,\"%s\",%u,0,0\r\n", topic, (unsigned)len);

    ESP_ClearBuffer();
    USART2_SendString(cmd);

    t = 0;
    while (t < wait_prompt_ms) {
        if (strchr(esp_rx_buffer, '>') != NULL) {
            break;
        }
        if (strstr(esp_rx_buffer, "ERROR") != NULL) {
            return 0;
        }
        Delay_ms(1);
        t++;
    }
    if (strchr(esp_rx_buffer, '>') == NULL) {
        return 0;
    }

    ESP_ClearBuffer();

    for (const char *p = payload; *p != '\0'; p++) {
        USART2_SendChar(*p);
    }

    t = 0;
    while (t < wait_ok_ms) {
        if (strstr(esp_rx_buffer, "+MQTTPUB:OK") != NULL ||
            strstr(esp_rx_buffer, "OK") != NULL) {
            return 1;
        }
        if (strstr(esp_rx_buffer, "+MQTTPUB:FAIL") != NULL ||
            strstr(esp_rx_buffer, "+MQTTDISCONNECTED") != NULL) {
            return 0;
        }
        if (strstr(esp_rx_buffer, "ERROR") != NULL) {
            return 0;
        }
        Delay_ms(1);
        t++;
    }
    return 0;
}

