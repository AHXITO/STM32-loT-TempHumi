#include "stm32f10x.h"
#include "Delay.h"
#include "OLED.h"
#include "DHT11.h"
#include "esp8266.h"
#include "usart.h"
#include "usart2.h"
#include "buzzer.h"
#include "mqtt.h"
#include "ringbuffer.h"
#include <string.h>

#define UART_BRIDGE_MODE 0

float temp = 0.0f;
uint8_t humi = 0;
uint8_t wifi_ok = 0, mqtt_ok = 0;
uint8_t dht_fail_count = 0;
uint8_t last_dht_valid = 0;

RingBuffer_t data_buffer;

int main(void)
{

    uint32_t last_dht = 0;          // 上一次 DHT11 读取时间
    uint32_t last_upload = 0;       // 上一次 MQTT 数据上传时间
    uint32_t last_display = 0;      // 上一次 OLED 显示刷新时间
    uint32_t last_th_sync = 0;      // 上一次阈值同步时间
    uint32_t last_mqtt_try = 0;     // 上一次尝试重连 MQTT 的时间
    uint32_t last_wifi_check = 0;   // 上一次 WiFi 状态检查时间

    uint32_t wifi_became_ok_at = 0; // WiFi 刚刚恢复连接的时间戳
    uint32_t mqtt_became_ok_at = 0; // MQTT 刚刚连接成功的时间戳

    uint8_t wifi_lost_confirm = 0;  // WiFi 掉线确认计数（连续2次才认定掉线）
    uint8_t mqtt_pub_fail_cnt = 0;  // MQTT 发布失败计数（连续3次才判定失效）

    Delay_Init();
    OLED_Init();
    USART1_Init(115200);
    USART2_Init(115200);

    Buzzer_Init();
    RingBuffer_Init(&data_buffer);

    OLED_ShowString(1,1,"System Starting...");
    Delay_ms(1000);

#if UART_BRIDGE_MODE
    OLED_Clear();
    OLED_ShowString(1,1,"AT Bridge Mode");
    OLED_ShowString(2,1,"U1<->ESP(U2)");
    USART1_SendString("\r\n=== AT Bridge Mode ===\r\n");
    USART1_SendString("Input AT on USART1, forwarded to ESP(USART2).\r\n");
    USART1_SendString("Set UART_BRIDGE_MODE to 0 to resume app.\r\n\r\n");

    while(1)
    {
        if (usart1_rx_index > 0) {
            uint16_t i;
            uint8_t has_line_end = 0;

            for (i = 0; i < usart1_rx_index; i++) {
                if (usart1_rx_buffer[i] == '\r' || usart1_rx_buffer[i] == '\n') {
                    has_line_end = 1;
                    break;
                }
            }


            if (has_line_end || usart1_rx_index >= (USART1_BUF_SIZE - 2)) {
                USART2_SendString(usart1_rx_buffer);
                USART1_ClearBuffer();
            }
        }

        if (esp_rx_index > 0) {
            USART1_SendString(esp_rx_buffer);
            ESP_ClearBuffer();
        }

        Delay_ms(5);
    }
#endif

    // WiFi 初始化
    ESP8266_Init();
    wifi_ok = 1;
    USART1_SendString("WiFi OK\r\n");

    // MQTT 初始化
    if (MQTT_Init()) {
        mqtt_ok = 1;
        mqtt_became_ok_at = Get_SystemTick();
        USART1_SendString("MQTT OK\r\n");
    }

    OLED_Clear();

    while(1)
    {
        uint32_t now = Get_SystemTick();

        /* WiFi 状态维护：MQTT 在线时降低探测频率，减少 AT 干扰 */
        if (now - last_wifi_check >= (mqtt_ok ? 30000u : 5000u)) {
            uint8_t prev_wifi = wifi_ok;
            last_wifi_check = now;
            ESP8266_MaintainWiFi(&wifi_ok);
            if (!wifi_ok && prev_wifi) {
                /* 去抖：连续两次检测到掉线再判定，避免瞬时 AT 响应抖动 */
                wifi_lost_confirm++;
                if (wifi_lost_confirm >= 2) {
                    mqtt_ok = 0;
                    USART1_SendString("WiFi lost confirmed, reset MQTT.\r\n");
                    wifi_lost_confirm = 0;
                }
            } else if (wifi_ok && !prev_wifi) {
                wifi_became_ok_at = now;
                wifi_lost_confirm = 0;
            } else {
                wifi_lost_confirm = 0;
            }
        }

        // 1. DHT11 读取
        if (now - last_dht >= 1000) {
            last_dht = now;

            if (DHT11_Read_Data(&temp, &humi)) {
                last_dht_valid = 1;
                dht_fail_count = 0;
                SensorData_t d = {temp, humi};
                (void)RingBuffer_Push(&data_buffer, d);
            } else {
                last_dht_valid = 0;
                dht_fail_count++;
            }
        }

        /* 2. 断网缓存与补发：队列非空且 MQTT 在线时按节流补发，成功才出队 */
        if (mqtt_ok &&
            (now - mqtt_became_ok_at >= 5000) &&
            (now - last_upload >= 5000)) {
            SensorData_t d;
            if (RingBuffer_Peek(&data_buffer, &d)) {
                last_upload = now;
                if (MQTT_Upload(d.temp, d.humi)) {
                    (void)RingBuffer_Pop(&data_buffer, &d);
                    mqtt_pub_fail_cnt = 0;
                } else {
                    /* 抖动容错：连续 3 次失败才判定 MQTT 失效，避免单次网络波动触发重连风暴 */
                    if (++mqtt_pub_fail_cnt >= 3) {
                        mqtt_ok = 0;
                        mqtt_pub_fail_cnt = 0;
                    }
                }
            }
        }

        /* 阈值同步：1) 下发修改后尽快同步一次；2) 每 60s 心跳同步一次 */
        if (mqtt_ok && (now - mqtt_became_ok_at >= 5000)) {
            uint8_t need_sync = MQTT_ConsumeThresholdDirty();
            if (need_sync || (now - last_th_sync >= 60000)) {
                if (MQTT_UploadThreshold()) {
                    last_th_sync = now;
                } else {
                    if (++mqtt_pub_fail_cnt >= 3) {
                        mqtt_ok = 0;
                        mqtt_pub_fail_cnt = 0;
                    }
                }
            }
        }

        // 3. OLED 显示
        if (now - last_display >= 500) {
            last_display = now;

            OLED_ShowString(1, 1, "Temp:  C   ");
            {
                int32_t ti = (int32_t)temp;
                if (ti >= 0) {
                    OLED_ShowNum(1, 7, (uint32_t)ti, 2);
                } else {
                    OLED_ShowChar(1, 7, '-');
                    if (ti <= -10) {
                        OLED_ShowNum(1, 8, (uint32_t)(-ti), 2);
                    } else {
                        OLED_ShowNum(1, 8, (uint32_t)(-ti), 1);
                        OLED_ShowChar(1, 9, ' ');
                    }
                }
            }

            OLED_ShowString(2,1,"Humi:  %   ");
            OLED_ShowNum(2,7,humi,2);

            OLED_ShowString(3,1,"WiFi: ");
            OLED_ShowString(3,7, wifi_ok ? "OK " : "NO ");
            OLED_ShowString(4,1,"MQTT: ");
            OLED_ShowString(4,7, mqtt_ok ? "OK" : "NO");
        }

        /* 4. 阈值报警：超过阈值蜂鸣器报警（阈值可被 MQTT 下发实时修改） */
        if (last_dht_valid &&
            (temp >= temp_threshold_high || humi >= humi_threshold_high)) {
            Buzzer_On();
        } else {
            Buzzer_Off();
        }

        /* 掉线/连接失败自动重试，避免卡在初始化阶段 */
        /* WiFi 刚恢复时给 TCP/IP 一点稳定时间 */
        if (wifi_ok && !mqtt_ok &&
            (now - last_mqtt_try >= 5000) &&
            (wifi_became_ok_at == 0 || (now - wifi_became_ok_at >= 3000))) {
            last_mqtt_try = now;
            if (MQTT_Init()) {
                mqtt_ok = 1;
                mqtt_became_ok_at = now;
                mqtt_pub_fail_cnt = 0;
                last_th_sync = 0;
                USART1_SendString("MQTT OK\r\n");
            }
        }

        {
            uint8_t prev_mqtt = mqtt_ok;
            MQTT_Maintain(&mqtt_ok);
            if (!prev_mqtt && mqtt_ok) {
                mqtt_became_ok_at = now;
                mqtt_pub_fail_cnt = 0;
                last_th_sync = 0;
            }
        }
        MQTT_ProcessReceived();

        Delay_ms(10);
    }
}
