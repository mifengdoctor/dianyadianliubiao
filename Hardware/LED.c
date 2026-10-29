#include "LED.h"
#include "stm32f10x.h"

/* LED1接PA12，低电平点亮。 */
void LED_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_SetBits(GPIOA, GPIO_Pin_12);
}

void LED1_ON(void)
{
    GPIO_ResetBits(GPIOA, GPIO_Pin_12);
}

void LED1_OFF(void)
{
    GPIO_SetBits(GPIOA, GPIO_Pin_12);
}

void LED1_Turn(void)
{
    if (GPIO_ReadOutputDataBit(GPIOA, GPIO_Pin_12) == RESET)
    {
        GPIO_SetBits(GPIOA, GPIO_Pin_12);
    }
    else
    {
        GPIO_ResetBits(GPIOA, GPIO_Pin_12);
    }
}
