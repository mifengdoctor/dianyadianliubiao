#include "Key.h"
#include "stm32f10x.h"

static volatile uint8_t key_events = 0U;

void Key_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
}

/* 1ms调用一次，20ms消抖。按下为低电平。 */
void Key_UpdateISR(void)
{
    static uint8_t sample_state = 0U;
    static uint8_t stable_state = 0U;
    static uint8_t debounce_count[3] = {0U, 0U, 0U};
    uint8_t raw_state = 0U;
    uint8_t i;

    if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_9) == RESET)
    {
        raw_state |= 0x01U;
    }
    if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_10) == RESET)
    {
        raw_state |= 0x02U;
    }
    if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_11) == RESET)
    {
        raw_state |= 0x04U;
    }

    for (i = 0U; i < 3U; i++)
    {
        uint8_t mask = (uint8_t)(1U << i);
        if ((raw_state & mask) != (sample_state & mask))
        {
            sample_state ^= mask;
            debounce_count[i] = 0U;
        }
        else if (debounce_count[i] < 20U)
        {
            debounce_count[i]++;
        }

        if (debounce_count[i] >= 20U)
        {
            if ((sample_state & mask) != (stable_state & mask))
            {
                stable_state ^= mask;
                if ((stable_state & mask) != 0U)
                {
                    key_events |= mask;
                }
            }
        }
    }
}

uint8_t Key_GetEvents(void)
{
    uint8_t events;
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    events = key_events;
    key_events = 0U;
    if (primask == 0U)
    {
        __enable_irq();
    }
    return events;
}
