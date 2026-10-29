#include "stm32f10x.h"
#include "Delay.h"

/* 基于SysTick的阻塞延时，自动跟随SystemCoreClock。 */
void Delay_us(uint32_t us)
{
    uint32_t ticks;

    if (us == 0U)
    {
        return;
    }

    ticks = (SystemCoreClock / 1000000U) * us;
    if (ticks == 0U)
    {
        ticks = 1U;
    }
    if (ticks > 0x00FFFFFFU)
    {
        ticks = 0x00FFFFFFU;
    }

    SysTick->LOAD = ticks - 1U;
    SysTick->VAL = 0U;
    SysTick->CTRL = 0x00000005U;              /* HCLK，无中断 */
    while ((SysTick->CTRL & 0x00010000U) == 0U)
    {
    }
    SysTick->CTRL = 0x00000004U;
}

void Delay_ms(uint32_t ms)
{
    while (ms--)
    {
        Delay_us(1000U);
    }
}

void Delay_s(uint32_t s)
{
    while (s--)
    {
        Delay_ms(1000U);
    }
}
