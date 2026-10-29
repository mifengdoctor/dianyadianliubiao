#include "Calibration.h"
#include "stm32f10x.h"

/* STM32F103C8T6为64KB Flash，最后一页地址为0x0800FC00。 */
#define CAL_PAGE_ADDRESS   0x0800FC00UL
#define CAL_MAGIC          0xA55AU

/* 未校准时按原硬件参数估算：电压分压220k/10k，电流采样100mR，ADC参考3.3V。 */
#define DEFAULT_V5_RAW     270U
#define DEFAULT_V15_RAW    809U
#define DEFAULT_I05_RAW    62U
#define DEFAULT_I15_RAW    186U
#define MIN_V5_RAW         50U
#define MIN_I05_RAW        10U

static uint16_t X05;
static uint16_t X15;
static uint16_t IX05;
static uint16_t IX15;
static float K;
static float KI;

static void Calibration_Compute(void)
{
    if (X15 > X05)
    {
        K = 10.0f / (float)(X15 - X05);       /* 5V到15V */
    }
    else
    {
        K = 0.0f;
    }

    if (IX15 > IX05)
    {
        KI = 100.0f / (float)(IX15 - IX05);   /* 0.5A到1.5A */
    }
    else
    {
        KI = 0.0f;
    }
}

static void Calibration_SetDefaults(void)
{
    X05 = DEFAULT_V5_RAW;
    X15 = DEFAULT_V15_RAW;
    IX05 = DEFAULT_I05_RAW;
    IX15 = DEFAULT_I15_RAW;
}

static uint8_t Calibration_ValuesValid(void)
{
    if ((X05 < MIN_V5_RAW) || (X15 <= (X05 + 5U)))
    {
        return 0U;
    }
    if ((IX05 < MIN_I05_RAW) || (IX15 <= (IX05 + 5U)))
    {
        return 0U;
    }
    return 1U;
}

static uint8_t Calibration_WriteFlash(void)
{
    uint16_t data[5];
    uint32_t address = CAL_PAGE_ADDRESS;
    uint8_t i;
    uint8_t result = 1U;

    data[0] = CAL_MAGIC;
    data[1] = X05;
    data[2] = X15;
    data[3] = IX05;
    data[4] = IX15;

    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
    if (FLASH_ErasePage(CAL_PAGE_ADDRESS) == FLASH_COMPLETE)
    {
        for (i = 0U; i < 5U; i++)
        {
            if (FLASH_ProgramHalfWord(address, data[i]) != FLASH_COMPLETE)
            {
                result = 0U;
                break;
            }
            address += 2U;
        }
    }
    else
    {
        result = 0U;
    }
    FLASH_Lock();

    return result;
}

void Calibration_Init(void)
{
    const uint16_t *flash = (const uint16_t *)CAL_PAGE_ADDRESS;
    uint8_t use_defaults = 0U;

    if (flash[0] == CAL_MAGIC)
    {
        X05 = flash[1];
        X15 = flash[2];
        IX05 = flash[3];
        IX15 = flash[4];
        if (Calibration_ValuesValid() == 0U)
        {
            use_defaults = 1U;
        }
    }
    else
    {
        use_defaults = 1U;
    }

    if (use_defaults != 0U)
    {
        Calibration_SetDefaults();
        (void)Calibration_WriteFlash();          /* 首次上电或参数损坏时写入默认值 */
    }

    Calibration_Compute();
}

uint8_t Calibration_SavePoint(uint8_t mode, uint16_t raw_voltage, uint16_t raw_current)
{
    uint16_t old_x05 = X05;
    uint16_t old_x15 = X15;
    uint16_t old_ix05 = IX05;
    uint16_t old_ix15 = IX15;
    uint16_t new_x05 = X05;
    uint16_t new_x15 = X15;
    uint16_t new_ix05 = IX05;
    uint16_t new_ix15 = IX15;

    switch (mode)
    {
        case CAL_MODE_VOLT_5V:
            new_x05 = raw_voltage;
            break;
        case CAL_MODE_VOLT_15V:
            new_x15 = raw_voltage;
            break;
        case CAL_MODE_CURR_05A:
            new_ix05 = raw_current;
            break;
        case CAL_MODE_CURR_15A:
            new_ix15 = raw_current;
            break;
        default:
            return 0U;
    }

    X05 = new_x05;
    X15 = new_x15;
    IX05 = new_ix05;
    IX15 = new_ix15;
    if (Calibration_ValuesValid() == 0U)
    {
        X05 = old_x05;
        X15 = old_x15;
        IX05 = old_ix05;
        IX15 = old_ix15;
        return 0U;
    }

    Calibration_Compute();
    if (Calibration_WriteFlash() == 0U)
    {
        X05 = old_x05;
        X15 = old_x15;
        IX05 = old_ix05;
        IX15 = old_ix15;
        Calibration_Compute();
        return 0U;
    }

    return 1U;
}

/* 输出单位：电压10mV，电流10mA。 */
void Calibration_Convert(uint16_t raw_voltage, uint16_t raw_current,
                         uint32_t *voltage_10mv, uint32_t *current_10ma)
{
    float value;

    if (raw_voltage >= X05)
    {
        value = (K * (float)(raw_voltage - X05) + 5.0f) * 1000.0f;
    }
    else if (X05 != 0U)
    {
        value = (5000.0f / (float)X05) * (float)raw_voltage;
    }
    else
    {
        value = 0.0f;
    }
    if (value < 0.0f)
    {
        value = 0.0f;
    }
    *voltage_10mv = ((uint32_t)(value + 0.5f) + 5U) / 10U;

    /*
     * 电流始终使用0.5A/1.5A两点拟合出的完整直线，低电流段也保留截距。
     * 原先低于0.5A时强制按零点重新计算，会将采样零点偏移放大成固定电流误差。
     */
    value = (KI * (float)(raw_current - IX05) + 50.0f) * 10.0f;
    if (value < 0.0f)
    {
        value = 0.0f;
    }
    *current_10ma = ((uint32_t)(value + 0.5f) + 5U) / 10U;
}
