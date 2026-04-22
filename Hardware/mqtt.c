#include "mqtt.h"
#include "esp8266.h"
#include "usart.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "usart2.h"
#include "Delay.h"

float temp_threshold_high = 30.0f;
uint8_t humi_threshold_high = 80;
static uint8_t threshold_dirty = 1;

#define PRODUCT_ID   "vH1u33xJS3"
#define DEVICE_NAME  "dht11"
#define DEVICE_KEY   "version=2018-10-31&res=products%2FvH1u33xJS3%2Fdevices%2Fdht11&et=1815285540&method=md5&sign=Zd7RvBPqLmXvdNnuVjhhMw%3D%3D"
#define MQTT_HOST              "mqtts.heclouds.com"
#define MQTT_PORT              1883
#define MQTT_CONN_LAST_PARAM   1

static uint8_t mqtt_send_cmd_retry(char *cmd, uint16_t timeout_ms, uint8_t retry)
{
    while (retry--) {
        if (ESP8266_SendCmd(cmd, "OK", timeout_ms)) {
            return 1;
        }
        /* 某些固件会返回已连接/已订阅等提示，按成功处理 */
        if (strstr(esp_rx_buffer, "ALREADY CONNECTED") != NULL) {
            return 1;
        }
        /* 部分固件重复订阅会返回 ALREADY SUBSCRIBE，这里视为成功 */
        if (strstr(esp_rx_buffer, "ALREADY SUBSCRIBE") != NULL) {
            return 1;
        }
        if (strstr(esp_rx_buffer, "+MQTTDISCONNECTED") != NULL) {
            /* 连接过程掉线：退避后重试 */
            Delay_ms(800);
        }
        Delay_ms(300);
    }
    return 0;
}

static uint8_t mqtt_wait_net_ready(uint8_t retry)
{
    while (retry--) {
        /* STA 仍然连着 AP */
        if (!ESP8266_IsWiFiConnected()) {
            Delay_ms(800);
            continue;
        }

        /*已拿到有效 IP（避免 WiFi 刚恢复时立刻 MQTTCONN 失败） */
        if (ESP8266_SendCmd("AT+CIPSTA?\r\n", "OK", 2000)) {
            if (strstr(esp_rx_buffer, "ip:\"0.0.0.0\"") == NULL &&
                strstr(esp_rx_buffer, "ip:\"") != NULL) {
                return 1;
            }
        }
        Delay_ms(800);
    }
    return 0;
}

uint8_t MQTT_Init(void)
{
    char cmd[512];

    USART1_SendString("=== MQTT Init Start ===\r\n");

    /* 保险：清理上一轮可能残留的会话/配置状态*/
    (void)ESP8266_SendCmd("AT+MQTTDISCONN\r\n", "OK", 1500);
    (void)ESP8266_SendCmd("AT+MQTTCLEAN=0\r\n", "OK", 2000);
    Delay_ms(200);

    if (!mqtt_wait_net_ready(8)) {
        USART1_SendString("NET not ready before MQTT init\r\n");
        return 0;
    }

    /* Token 中含 %2F、%3D 等，禁止用 sprintf 整串写入，否则会按格式符解析 */
    strcpy(cmd, "AT+MQTTUSERCFG=0,1,\"");
    strcat(cmd, DEVICE_NAME);
    strcat(cmd, "\",\"");
    strcat(cmd, PRODUCT_ID);
    strcat(cmd, "\",\"");
    strcat(cmd, DEVICE_KEY);
    strcat(cmd, "\",0,0,\"\"\r\n");

    USART1_SendString("Sending MQTTUSERCFG...\r\n");
    if (!mqtt_send_cmd_retry(cmd, 12000, 3)) {
        USART1_SendString("MQTTUSERCFG Failed!\r\n");
        USART1_SendString("ESP Return:\r\n");
        USART1_SendString(esp_rx_buffer);
        USART1_SendString("\r\n");
        return 0;
    }
    USART1_SendString("MQTTUSERCFG OK\r\n");

    USART1_SendString("Connecting to OneNET...\r\n");
    sprintf(cmd, "AT+MQTTCONN=0,\"%s\",%d,%d\r\n", MQTT_HOST, MQTT_PORT, MQTT_CONN_LAST_PARAM);
    {
        uint8_t i;
        uint8_t conn_ok = 0;
        for (i = 0; i < 4; i++) {
            if (ESP8266_SendCmd(cmd, "OK", 25000)) {
                conn_ok = 1;
                break;
            }
            /* 遇到掉线时先等网络栈稳定再重试 */
            if (strstr(esp_rx_buffer, "+MQTTDISCONNECTED") != NULL ||
                strstr(esp_rx_buffer, "ERROR") != NULL) {
                Delay_ms(1200 + i * 600);
                (void)mqtt_wait_net_ready(3);
            }
        }
        if (!conn_ok) {
            USART1_SendString("MQTTCONN Failed!\r\n");
            USART1_SendString("ESP Return:\r\n");
            USART1_SendString(esp_rx_buffer);
            USART1_SendString("\r\n");
            return 0;
        }
    }

    USART1_SendString("MQTTCONN Success! Connected to OneNET\r\n");
    /* 给模块一点时间进入稳定连接态，减少紧接 MQTTSUB 的偶发失败 */
    Delay_ms(800);

    sprintf(cmd, "AT+MQTTSUB=0,\"$sys/%s/%s/thing/property/post/reply\",0\r\n", PRODUCT_ID, DEVICE_NAME);
    if (!mqtt_send_cmd_retry(cmd, 5000, 3)) {
        USART1_SendString("MQTTSUB post/reply failed\r\n");
        USART1_SendString(esp_rx_buffer);
        USART1_SendString("\r\n");
        return 0;
    }

    Delay_ms(200);
    sprintf(cmd, "AT+MQTTSUB=0,\"$sys/%s/%s/thing/property/set\",0\r\n", PRODUCT_ID, DEVICE_NAME);
    if (!mqtt_send_cmd_retry(cmd, 5000, 3)) {
        USART1_SendString("MQTTSUB set failed\r\n");
        USART1_SendString(esp_rx_buffer);
        USART1_SendString("\r\n");
        return 0;
    }

    USART1_SendString("MQTT Init Complete! Ready to upload data.\r\n");
    return 1;
}

uint8_t MQTT_Reconnect(void)
{
    /* 仅在掉线重连时断开，正常运行不调用 MQTTCLEAN */
    ESP8266_SendCmd("AT+MQTTDISCONN\r\n", "OK", 2000);
    Delay_ms(1000);
    return MQTT_Init();
}

void MQTT_Maintain(uint8_t *mqtt_ok)
{
    if (mqtt_ok == NULL || !*mqtt_ok) {
        return;
    }
    if (strstr(esp_rx_buffer, "+MQTTDISCONNECTED") != NULL) {
        USART1_SendString("MQTT disconnected, reconnecting...\r\n");
        *mqtt_ok = 0;
        if (MQTT_Reconnect()) {
            *mqtt_ok = 1;
        }
        ESP_ClearBuffer();
    }
}

uint8_t MQTT_Upload(float temp, uint8_t humi)
{
    char topic[96];
    char json[220];
    char tstr[16];
    float ta = temp;
    int sign = 1;

    if (ta < 0.0f) {
        sign = -1;
        ta = -ta;
    }
    {
        int ti = (int)ta;
        int td = (int)((ta - (float)ti) * 10.0f + 0.5f);
        if (td >= 10) {
            td -= 10;
            ti++;
        }
        if (sign < 0) {
            sprintf(tstr, "-%d.%d", ti, td);
        } else {
            sprintf(tstr, "%d.%d", ti, td);
        }
    }

    sprintf(topic, "$sys/%s/%s/thing/property/post", PRODUCT_ID, DEVICE_NAME);
    /* 周期上报只发实时传感器值，减小载荷，降低 +MQTTPUB:FAIL 概率 */
    sprintf(json,
            "{\"id\":\"123\",\"version\":\"1.0\",\"params\":{"
            "\"humidity_value\":{\"value\":%d},"
            "\"temp_value\":{\"value\":%s}"
            "}}",
            humi, tstr);

    {
        uint8_t i;
        for (i = 0; i < 2; i++) {
            if (ESP8266_MqttPubRaw(topic, json, 5000, 9000)) {
                return 1;
            }
            Delay_ms(300);
        }
        /* 不再回退普通 MQTTPUB，避免在不稳定链路上触发 +MQTTPUB:FAIL 后直接断开 */
        USART1_SendString("MQTT Upload Failed(RAW)! ESP RX:\r\n");
        USART1_SendString(esp_rx_buffer);
        USART1_SendString("\r\n");
        return 0;
    }
}

//上报阈值
uint8_t MQTT_UploadThreshold(void)
{
    char topic[96];
    char json[220];
    char thstr[16];
    float th = temp_threshold_high;
    int sign = 1;

    if (th < 0.0f) {
        sign = -1;
        th = -th;
    }
    {
        int ti = (int)th;
        int td = (int)((th - (float)ti) * 10.0f + 0.5f);
        if (td >= 10) {
            td -= 10;
            ti++;
        }
        if (sign < 0) {
            sprintf(thstr, "-%d.%d", ti, td);
        } else {
            sprintf(thstr, "%d.%d", ti, td);
        }
    }

    sprintf(topic, "$sys/%s/%s/thing/property/post", PRODUCT_ID, DEVICE_NAME);
    sprintf(json,
            "{\"id\":\"124\",\"version\":\"1.0\",\"params\":{"
            "\"maxhum_set\":{\"value\":%u},"
            "\"maxtemp_set\":{\"value\":%s}"
            "}}",
            (unsigned)humi_threshold_high, thstr);

    /* 增加重试次数，提高同步成功率 */
    uint8_t i;
    for (i = 0; i < 3; i++) {
        if (ESP8266_MqttPubRaw(topic, json, 5000, 9000)) {
            threshold_dirty = 0;
            return 1;
        }
        Delay_ms(200);
    }
    /* 失败时保持threshold_dirty为1，下次继续尝试 */
    return 0;
}

uint8_t MQTT_ConsumeThresholdDirty(void)
{
    return threshold_dirty;
}

//从 ESP8266 收到的完整 MQTT 下发字符串中，提取出真正的JSON部分，跳过前面的 Topic 和长度信息。
static char *mqtt_json_payload_after_set_topic(char *recv)
{
    char *t = strstr(recv, "/thing/property/set\"");
    if (t == NULL) {
        return NULL;
    }
    t += strlen("/thing/property/set\"");
    if (*t != ',') {
        return NULL;
    }
    t++;
    (void)strtol(t, &t, 10);
    if (*t != ',') {
        return NULL;
    }
    return t + 1;
}

//处理 ESP8266 接收到的 MQTT 下发消息，解析温度/湿度阈值，更新全局变量，并回复确认消息。
void MQTT_ProcessReceived(void)
{
    char *recv = strstr(esp_rx_buffer, "+MQTTSUBRECV:");
    if (recv == NULL) {
        recv = strstr(esp_rx_buffer, "+MQTTRECV:");
    }
    if (recv == NULL || strstr(recv, "thing/property/set") == NULL) {
        return;
    }

    char *json = mqtt_json_payload_after_set_topic(recv);
    if (json == NULL) {
        json = recv;
    }

    char id_buf[20] = "1";
    char *idp = strstr(json, "\"id\"");
    if (idp != NULL) {
        idp = strchr(idp, ':');
        if (idp != NULL) {
            idp++;
            while (*idp == ' ' || *idp == '\t') {
                idp++;
            }
            if (*idp == '"') {
                idp++;
            }
            int k = 0;
            while (*idp && *idp != '"' && *idp != ',' && k < (int)sizeof(id_buf) - 1) {
                id_buf[k++] = *idp++;
            }
            id_buf[k] = '\0';
        }
    }

    /* 兼容两种下发格式：
       1) "maxhum_set":80
       2) "maxhum_set":{"value":80} */
    {
        uint8_t old_h = humi_threshold_high;
        float old_t = temp_threshold_high;
        char *p = strstr(json, "\"maxhum_set\"");
        if (p) {
            char *n = strstr(p, "\"value\"");
            if (n != NULL) {
                p = n;
            }
            p = strchr(p, ':');
            if (p != NULL) {
                long v;
                p++;
                while (*p == ' ' || *p == '\t' || *p == '{' || *p == '"') p++;
                v = strtol(p, NULL, 10);
                if (v < 0) v = 0;
                if (v > 100) v = 100;
                humi_threshold_high = (uint8_t)v;
            }
        }
        char *p2 = strstr(json, "\"maxtemp_set\"");
        if (p2) {
            char *n = strstr(p2, "\"value\"");
            if (n != NULL) {
                p2 = n;
            }
            p2 = strchr(p2, ':');
            if (p2 != NULL) {
                p2++;
                while (*p2 == ' ' || *p2 == '\t' || *p2 == '{' || *p2 == '"') p2++;
                int sign = 1;
                if (*p2 == '-') { sign = -1; p2++; }
                else if (*p2 == '+') { p2++; }

                long ip = 0;
                while (*p2 >= '0' && *p2 <= '9') {
                    ip = ip * 10 + (*p2 - '0');
                    p2++;
                }
                long dp = 0;
                if (*p2 == '.') {
                    p2++;
                    if (*p2 >= '0' && *p2 <= '9') {
                        dp = (*p2 - '0'); /* 只取 1 位小数 */
                    }
                }
                temp_threshold_high = (float)(sign * ip) + (float)(sign * dp) * 0.1f;
            }
        }
        if (old_h != humi_threshold_high || old_t != temp_threshold_high) {
            threshold_dirty = 1;
        }
    }

    USART1_SendString("Received command from OneNET!\r\n");
    {
        char logbuf[64];
        int t10 = (int)(temp_threshold_high * 10.0f);
        int ti = t10 / 10;
        int td = t10 >= 0 ? (t10 % 10) : (-(t10 % 10));
        sprintf(logbuf, "TH update: H=%u T=%d.%d\r\n", humi_threshold_high, ti, td);
        USART1_SendString(logbuf);
    }

    {
        char topic[96];
        char json[160];
        char cmd[256];
        sprintf(topic, "$sys/%s/%s/thing/property/set_reply", PRODUCT_ID, DEVICE_NAME);
        sprintf(json, "{\"id\":\"%s\",\"code\":200,\"msg\":\"success\"}", id_buf);
        if (!ESP8266_MqttPubRaw(topic, json, 5000, 8000)) {
            sprintf(cmd,
                    "AT+MQTTPUB=0,\"%s\",\"{\\\"id\\\":\\\"%s\\\",\\\"code\\\":200,\\\"msg\\\":\\\"success\\\"}\",0,0\r\n",
                    topic, id_buf);
            ESP8266_SendCmd(cmd, "OK", 3000);
        }
    }

    ESP_ClearBuffer();
}
