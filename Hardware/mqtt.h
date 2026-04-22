#ifndef __MQTT_H
#define __MQTT_H

#include "stm32f10x.h"

extern float   temp_threshold_high;
extern uint8_t humi_threshold_high;

uint8_t MQTT_Init(void);
uint8_t MQTT_Reconnect(void);
uint8_t MQTT_Upload(float temp, uint8_t humi);
uint8_t MQTT_UploadThreshold(void);
uint8_t MQTT_ConsumeThresholdDirty(void);
void MQTT_ProcessReceived(void);
void MQTT_Maintain(uint8_t *mqtt_ok);

#endif
