#include "stm32f10x.h"
#include "Delay.h"
#include "OLED.h"
#include "LED.h"
#include "Key.h"
#include "VoltageCurrent.h"
#include "Calibration.h"

static volatile uint32_t g_ms_tick = 0U;
static volatile uint8_t g_mode = CAL_MODE_MEASURE;

static uint16_t g_raw_voltage = 0U;
static uint16_t g_raw_current = 0U;
static uint32_t g_voltage_10mv = 0U;
static uint32_t g_current_10ma = 0U;
static uint8_t g_cal_failed = 0U;

static const char OLED_BLANK_LINE[] = "                ";

static void Timer2_Init(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    TIM_TimeBaseStructure.TIM_Period = 999U;
    TIM_TimeBaseStructure.TIM_Prescaler = 71U;       /* 72MHz / 7200 = 1kHz */
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);
    TIM_Cmd(TIM2, ENABLE);

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1U;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1U;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

static uint16_t Mean_Value_Filter(const uint16_t *value, uint32_t size)
{
    uint32_t sum = 0U;
    uint16_t max = 0U;
    uint16_t min = 0xFFFFU;
    uint32_t i;
    uint32_t primask;

    if (size < 3U)
    {
        return 0U;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    for (i = 0U; i < size; i++)
    {
        sum += value[i];
        if (value[i] > max)
        {
            max = value[i];
        }
        if (value[i] < min)
        {
            min = value[i];
        }
    }
    if (primask == 0U)
    {
        __enable_irq();
    }

    sum -= max;
    sum -= min;
    return (uint16_t)(sum / (size - 2U));
}

static void UpdateMeasurements(void)
{
    g_raw_voltage = Mean_Value_Filter(Volt_Buffer, ADC_SAMPLE_SIZE);
    g_raw_current = Mean_Value_Filter(Curr_Buffer, ADC_SAMPLE_SIZE);
    Calibration_Convert(g_raw_voltage, g_raw_current, &g_voltage_10mv, &g_current_10ma);
}

static void FormatVoltage(char *text, uint32_t value_10mv)
{
    uint32_t whole = value_10mv / 100U;
    uint32_t fraction = value_10mv % 100U;

    text[0] = 'V';
    text[1] = '=';
    text[2] = ' ';
    text[3] = (char)('0' + ((whole / 10U) % 10U));
    text[4] = (char)('0' + (whole % 10U));
    text[5] = '.';
    text[6] = (char)('0' + (fraction / 10U));
    text[7] = (char)('0' + (fraction % 10U));
    text[8] = ' ';
    text[9] = 'V';
    text[10] = '\0';
}

static void FormatCurrent(char *text, uint32_t value_10ma)
{
    uint32_t whole = value_10ma / 100U;
    uint32_t fraction = value_10ma % 100U;

    text[0] = 'I';
    text[1] = '=';
    text[2] = ' ';
    text[3] = (char)('0' + ((whole / 10U) % 10U));
    text[4] = (char)('0' + (whole % 10U));
    text[5] = '.';
    text[6] = (char)('0' + (fraction / 10U));
    text[7] = (char)('0' + (fraction % 10U));
    text[8] = ' ';
    text[9] = 'A';
    text[10] = '\0';
}

static void FormatPower(char *text, uint32_t voltage_10mv, uint32_t current_10ma)
{
    uint32_t power_cw = (voltage_10mv * current_10ma) / 100U;
    uint32_t whole = power_cw / 100U;
    uint32_t fraction = power_cw % 100U;

    text[0] = 'P';
    text[1] = '=';
    text[2] = ' ';
    text[3] = (char)('0' + ((whole / 10U) % 10U));
    text[4] = (char)('0' + (whole % 10U));
    text[5] = '.';
    text[6] = (char)('0' + (fraction / 10U));
    text[7] = (char)('0' + (fraction % 10U));
    text[8] = ' ';
    text[9] = 'W';
    text[10] = '\0';
}

static void ClearLine(uint8_t line)
{
    OLED_ShowString(line, 1U, OLED_BLANK_LINE);
}

static void ShowCalibrationScreen(void)
{
    char text[17];

    ClearLine(1U);
    ClearLine(2U);
    ClearLine(3U);
    ClearLine(4U);

    switch (g_mode)
    {
        case CAL_MODE_VOLT_5V:
            OLED_ShowString(1U, 1U, "CAL V  5.00 V");
            break;
        case CAL_MODE_VOLT_15V:
            OLED_ShowString(1U, 1U, "CAL V 15.00 V");
            break;
        case CAL_MODE_CURR_05A:
            OLED_ShowString(1U, 1U, "CAL I  0.50 A");
            break;
        case CAL_MODE_CURR_15A:
            OLED_ShowString(1U, 1U, "CAL I  1.50 A");
            break;
        default:
            break;
    }

    OLED_ShowString(2U, 1U, "ADC:");
    if ((g_mode == CAL_MODE_VOLT_5V) || (g_mode == CAL_MODE_VOLT_15V))
    {
        OLED_ShowNum(2U, 5U, g_raw_voltage, 4U);
    }
    else
    {
        OLED_ShowNum(2U, 5U, g_raw_current, 4U);
    }

    if ((g_mode == CAL_MODE_VOLT_5V) || (g_mode == CAL_MODE_VOLT_15V))
    {
        FormatVoltage(text, g_voltage_10mv);
        OLED_ShowString(3U, 1U, "Now ");
        OLED_ShowString(3U, 5U, text);
        if (g_voltage_10mv > 9999U)
        {
            OLED_ShowString(3U, 12U, " OVER");
        }
    }
    else
    {
        FormatCurrent(text, g_current_10ma);
        OLED_ShowString(3U, 1U, "Now ");
        OLED_ShowString(3U, 5U, text);
        if (g_current_10ma > 999U)
        {
            OLED_ShowString(3U, 12U, " OVER");
        }
    }

    if (g_cal_failed != 0U)
    {
        OLED_ShowString(4U, 1U, "CAL FAIL K3 Exit");
    }
    else
    {
        OLED_ShowString(4U, 1U, "K2 Save K3 Exit");
    }
}

static void ShowMeasurementScreen(void)
{
    char text[17];

    ClearLine(1U);
    ClearLine(2U);
    ClearLine(3U);
    ClearLine(4U);

    FormatVoltage(text, g_voltage_10mv);
    OLED_ShowString(1U, 1U, text);
    FormatCurrent(text, g_current_10ma);
    OLED_ShowString(2U, 1U, text);
    FormatPower(text, g_voltage_10mv, g_current_10ma);
    OLED_ShowString(3U, 1U, text);
    OLED_ShowString(4U, 1U, "K1: Menu");
}

static void ShowCurrentScreen(void)
{
    if (g_mode == CAL_MODE_MEASURE)
    {
        ShowMeasurementScreen();
    }
    else
    {
        ShowCalibrationScreen();
    }
}

static void HandleKeys(uint8_t events)
{
    uint16_t raw_voltage;
    uint16_t raw_current;

    if ((events & KEY_EVENT_K1) != 0U)
    {
        g_mode++;
        if (g_mode > CAL_MODE_CURR_15A)
        {
            g_mode = CAL_MODE_MEASURE;
        }
        g_cal_failed = 0U;
    }

    if ((events & KEY_EVENT_K2) != 0U)
    {
        if (g_mode != CAL_MODE_MEASURE)
        {
            raw_voltage = Mean_Value_Filter(Volt_Buffer, ADC_SAMPLE_SIZE);
            raw_current = Mean_Value_Filter(Curr_Buffer, ADC_SAMPLE_SIZE);
            if (Calibration_SavePoint(g_mode, raw_voltage, raw_current) != 0U)
            {
                g_mode = CAL_MODE_MEASURE;
                g_cal_failed = 0U;
            }
            else
            {
                g_cal_failed = 1U;
            }
        }
    }

    if ((events & KEY_EVENT_K3) != 0U)
    {
        if (g_mode != CAL_MODE_MEASURE)
        {
            g_mode = CAL_MODE_MEASURE;
            g_cal_failed = 0U;
        }
    }
}

int main(void)
{
    uint32_t last_display_tick;
    uint32_t now;
    uint8_t events;

    OLED_Init();
    LED_Init();
    Key_Init();
    VC_Init();
    Calibration_Init();
    Timer2_Init();

    UpdateMeasurements();
    ShowCurrentScreen();
    last_display_tick = g_ms_tick;

    while (1)
    {
        events = Key_GetEvents();
        if (events != 0U)
        {
            HandleKeys(events);
            UpdateMeasurements();
            ShowCurrentScreen();
            last_display_tick = g_ms_tick;
        }

        now = g_ms_tick;
        if ((uint32_t)(now - last_display_tick) >= 200U)
        {
            last_display_tick = now;
            UpdateMeasurements();
            ShowCurrentScreen();
        }
    }
}

void TIM2_IRQHandler(void)
{
    static uint32_t led_tick = 0U;

    if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET)
    {
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);

        g_ms_tick++;
        VC_TimerSample();
        Key_UpdateISR();

        if (g_mode == CAL_MODE_MEASURE)
        {
            led_tick++;
            if (led_tick >= 1000U)
            {
                led_tick = 0U;
                LED1_Turn();
            }
        }
        else
        {
            led_tick = 0U;
            LED1_ON();
        }
    }
}
