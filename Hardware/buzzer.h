#ifndef __BUZZER_H
#define __BUZZER_H


#define BUZZER_GPIO  GPIOB
#define BUZZER_PIN   GPIO_Pin_0

void Buzzer_Init(void);
void Buzzer_On(void);
void Buzzer_Off(void);

#endif
