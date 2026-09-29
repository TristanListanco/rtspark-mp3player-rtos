/*
 * board_init.c - Clock tree, peripheral initialisation and HAL MSP callbacks
 * for the RT-Spark MP3 player.
 *
 * Written in the structure STM32CubeMX generates (SystemClock_Config,
 * MX_xxx_Init, HAL_xxx_MspInit) for this configuration:
 *   SYSCLK 168 MHz from HSE 8 MHz (PLLM 4, PLLN 168, PLLP 2, PLLQ 7)
 *   AHB 168 MHz, APB1 42 MHz (timers 84 MHz), APB2 84 MHz
 *   PLLI2S: N 192, R 2 -> I2SCLK 192 MHz
 *   FSMC NOR/SRAM bank 3 (LCD), I2C2 (codec control), I2S3 master TX + DMA1
 *   Stream 7 (codec audio), ADC3 IN4 (potentiometer), USART1 (console)
 */
#include "main.h"

ADC_HandleTypeDef   hadc3;
I2C_HandleTypeDef   hi2c2;
I2S_HandleTypeDef   hi2s3;
DMA_HandleTypeDef   hdma_spi3_tx;
UART_HandleTypeDef  huart1;
SRAM_HandleTypeDef  hsram3;

/* ============================ Clocks ===================================== */

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};
    RCC_PeriphCLKInitTypeDef periph = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState = RCC_HSE_ON;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM = 4;
    osc.PLL.PLLN = 168;
    osc.PLL.PLLP = RCC_PLLP_DIV2;
    osc.PLL.PLLQ = 7;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        Error_Handler();
    }

    clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                    RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV4;
    clk.APB2CLKDivider = RCC_HCLK_DIV2;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_5) != HAL_OK) {
        Error_Handler();
    }

    /* I2S clock: 8 MHz / 4 * 192 / 2 = 192 MHz */
    periph.PeriphClockSelection = RCC_PERIPHCLK_I2S;
    periph.PLLI2S.PLLI2SN = 192;
    periph.PLLI2S.PLLI2SR = 2;
    if (HAL_RCCEx_PeriphCLKConfig(&periph) != HAL_OK) {
        Error_Handler();
    }
}

/* ============================ GPIO ======================================= */

static void gpio_output(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState initial)
{
    GPIO_InitTypeDef g = {0};
    HAL_GPIO_WritePin(port, pin, initial);
    g.Pin = pin;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(port, &g);
}

static void gpio_input_pullup(GPIO_TypeDef *port, uint16_t pin)
{
    /* The internal pull-up (~40k) is in parallel with the external 330R one;
     * it keeps the input defined if the external resistor is missing. */
    GPIO_InitTypeDef g = {0};
    g.Pin = pin;
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(port, &g);
}

void MX_GPIO_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();

    /* Outputs, all LEDs off */
    const GPIO_PinState rgb_off = RGB_LED_COMMON_ANODE ? GPIO_PIN_SET : GPIO_PIN_RESET;
    gpio_output(LED_RED_GPIO_Port, LED_RED_Pin, rgb_off);
    gpio_output(LED_GREEN_GPIO_Port, LED_GREEN_Pin, rgb_off);
    gpio_output(LED_BLUE_GPIO_Port, LED_BLUE_Pin, rgb_off);
    gpio_output(BOARD_LED_R_GPIO_Port, BOARD_LED_R_Pin, GPIO_PIN_SET);   /* active low */
    gpio_output(BOARD_LED_B_GPIO_Port, BOARD_LED_B_Pin, GPIO_PIN_SET);
    gpio_output(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_SET);
    gpio_output(LCD_BL_GPIO_Port, LCD_BL_Pin, GPIO_PIN_RESET);           /* backlight off */

    /* Inputs (active low) */
    gpio_input_pullup(BTN1_GPIO_Port, BTN1_Pin);
    gpio_input_pullup(BTN2_GPIO_Port, BTN2_Pin);
    gpio_input_pullup(BTN3_GPIO_Port, BTN3_Pin);
    gpio_input_pullup(BTN4_GPIO_Port, BTN4_Pin);
    gpio_input_pullup(USER_BTN_GPIO_Port, USER_BTN_Pin);
}

/* ============================ DMA ======================================== */

void MX_DMA_Init(void)
{
    __HAL_RCC_DMA1_CLK_ENABLE();
    /* DMA1 Stream 7 channel 0 = SPI3_TX / I2S3 */
    HAL_NVIC_SetPriority(DMA1_Stream7_IRQn, AUDIO_IRQ_PRIORITY, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream7_IRQn);
}

/* ============================ USART1 ===================================== */

void MX_USART1_UART_Init(void)
{
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart1) != HAL_OK) {
        Error_Handler();
    }
}

/* ============================ FSMC (LCD) ================================= */

void MX_FSMC_Init(void)
{
    FSMC_NORSRAM_TimingTypeDef read = {0};
    FSMC_NORSRAM_TimingTypeDef write = {0};

    hsram3.Instance = FSMC_NORSRAM_DEVICE;
    hsram3.Extended = FSMC_NORSRAM_EXTENDED_DEVICE;
    hsram3.Init.NSBank = FSMC_NORSRAM_BANK3;
    hsram3.Init.DataAddressMux = FSMC_DATA_ADDRESS_MUX_DISABLE;
    hsram3.Init.MemoryType = FSMC_MEMORY_TYPE_SRAM;
    hsram3.Init.MemoryDataWidth = FSMC_NORSRAM_MEM_BUS_WIDTH_8;
    hsram3.Init.BurstAccessMode = FSMC_BURST_ACCESS_MODE_DISABLE;
    hsram3.Init.WaitSignalPolarity = FSMC_WAIT_SIGNAL_POLARITY_LOW;
    hsram3.Init.WrapMode = FSMC_WRAP_MODE_DISABLE;
    hsram3.Init.WaitSignalActive = FSMC_WAIT_TIMING_BEFORE_WS;
    hsram3.Init.WriteOperation = FSMC_WRITE_OPERATION_ENABLE;
    hsram3.Init.WaitSignal = FSMC_WAIT_SIGNAL_DISABLE;
    hsram3.Init.ExtendedMode = FSMC_EXTENDED_MODE_ENABLE;
    hsram3.Init.AsynchronousWait = FSMC_ASYNCHRONOUS_WAIT_DISABLE;
    hsram3.Init.WriteBurst = FSMC_WRITE_BURST_DISABLE;
    hsram3.Init.PageSize = FSMC_PAGE_SIZE_NONE;

    /* Read (ID only): slow, ST7789 read cycle >= 450 ns */
    read.AddressSetupTime = 15;
    read.AddressHoldTime = 0;
    read.DataSetupTime = 60;
    read.BusTurnAroundDuration = 0;
    read.CLKDivision = 0;
    read.DataLatency = 0;
    read.AccessMode = FSMC_ACCESS_MODE_A;

    /* Write: ~13 HCLK = 77 ns per byte (ST7789 write cycle >= 66 ns) */
    write.AddressSetupTime = 6;
    write.AddressHoldTime = 0;
    write.DataSetupTime = 6;
    write.BusTurnAroundDuration = 0;
    write.CLKDivision = 0;
    write.DataLatency = 0;
    write.AccessMode = FSMC_ACCESS_MODE_A;

    if (HAL_SRAM_Init(&hsram3, &read, &write) != HAL_OK) {
        Error_Handler();
    }
}

void HAL_SRAM_MspInit(SRAM_HandleTypeDef *hsram)
{
    GPIO_InitTypeDef g = {0};
    (void)hsram;

    __HAL_RCC_FSMC_CLK_ENABLE();

    g.Mode = GPIO_MODE_AF_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    g.Alternate = GPIO_AF12_FSMC;

    /* PE7..PE10 = D4..D7 */
    g.Pin = GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10;
    HAL_GPIO_Init(GPIOE, &g);

    /* PD0 D2, PD1 D3, PD4 NOE, PD5 NWE, PD13 A18 (RS), PD14 D0, PD15 D1 */
    g.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5 |
            GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOD, &g);

    /* PG10 = NE3 (LCD chip select) */
    g.Pin = GPIO_PIN_10;
    HAL_GPIO_Init(GPIOG, &g);
}

/* ============================ I2C2 (codec control) ======================= */

void MX_I2C2_Init(void)
{
    hi2c2.Instance = I2C2;
    hi2c2.Init.ClockSpeed = 100000;
    hi2c2.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c2.Init.OwnAddress1 = 0;
    hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c2.Init.OwnAddress2 = 0;
    hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c2) != HAL_OK) {
        Error_Handler();
    }
}

/* cppcheck-suppress constParameterPointer ; signature fixed by the HAL's weak callback */
void HAL_I2C_MspInit(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == I2C2) {
        GPIO_InitTypeDef g = {0};
        /* PF0 = I2C2_SDA, PF1 = I2C2_SCL (on-board pull-ups) */
        g.Pin = GPIO_PIN_0 | GPIO_PIN_1;
        g.Mode = GPIO_MODE_AF_OD;
        g.Pull = GPIO_PULLUP;
        g.Speed = GPIO_SPEED_FREQ_HIGH;
        g.Alternate = GPIO_AF4_I2C2;
        HAL_GPIO_Init(GPIOF, &g);

        __HAL_RCC_I2C2_CLK_ENABLE();
    }
}

/* ============================ I2S3 (codec audio) ========================= */

void MX_I2S3_Init(void)
{
    hi2s3.Instance = SPI3;
    hi2s3.Init.Mode = I2S_MODE_MASTER_TX;
    hi2s3.Init.Standard = I2S_STANDARD_PHILIPS;
    hi2s3.Init.DataFormat = I2S_DATAFORMAT_16B;
    hi2s3.Init.MCLKOutput = I2S_MCLKOUTPUT_ENABLE;   /* ES8388 needs MCLK = 256 Fs */
    hi2s3.Init.AudioFreq = I2S_AUDIOFREQ_32K;
    hi2s3.Init.CPOL = I2S_CPOL_LOW;
    hi2s3.Init.ClockSource = I2S_CLOCK_PLL;
    hi2s3.Init.FullDuplexMode = I2S_FULLDUPLEXMODE_DISABLE;
    if (HAL_I2S_Init(&hi2s3) != HAL_OK) {
        Error_Handler();
    }
}

void HAL_I2S_MspInit(I2S_HandleTypeDef *hi2s)
{
    if (hi2s->Instance == SPI3) {
        GPIO_InitTypeDef g = {0};
        __HAL_RCC_SPI3_CLK_ENABLE();

        g.Mode = GPIO_MODE_AF_PP;
        g.Pull = GPIO_NOPULL;
        g.Speed = GPIO_SPEED_FREQ_HIGH;
        g.Alternate = GPIO_AF6_SPI3;

        g.Pin = GPIO_PIN_7;                 /* PC7  = I2S3_MCK */
        HAL_GPIO_Init(GPIOC, &g);
        g.Pin = GPIO_PIN_15;                /* PA15 = I2S3_WS  */
        HAL_GPIO_Init(GPIOA, &g);
        g.Pin = GPIO_PIN_3 | GPIO_PIN_5;    /* PB3  = I2S3_CK, PB5 = I2S3_SD */
        HAL_GPIO_Init(GPIOB, &g);

        hdma_spi3_tx.Instance = DMA1_Stream7;
        hdma_spi3_tx.Init.Channel = DMA_CHANNEL_0;
        hdma_spi3_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
        hdma_spi3_tx.Init.PeriphInc = DMA_PINC_DISABLE;
        hdma_spi3_tx.Init.MemInc = DMA_MINC_ENABLE;
        hdma_spi3_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
        hdma_spi3_tx.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
        hdma_spi3_tx.Init.Mode = DMA_CIRCULAR;
        hdma_spi3_tx.Init.Priority = DMA_PRIORITY_HIGH;
        hdma_spi3_tx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
        if (HAL_DMA_Init(&hdma_spi3_tx) != HAL_OK) {
            Error_Handler();
        }
        __HAL_LINKDMA(hi2s, hdmatx, hdma_spi3_tx);
    }
}

/* ============================ ADC3 (potentiometer) ======================= */

void MX_ADC3_Init(void)
{
    ADC_ChannelConfTypeDef ch = {0};

    hadc3.Instance = ADC3;
    hadc3.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;   /* 84 / 4 = 21 MHz */
    hadc3.Init.Resolution = ADC_RESOLUTION_12B;
    hadc3.Init.ScanConvMode = DISABLE;
    hadc3.Init.ContinuousConvMode = DISABLE;
    hadc3.Init.DiscontinuousConvMode = DISABLE;
    hadc3.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc3.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc3.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc3.Init.NbrOfConversion = 1;
    hadc3.Init.DMAContinuousRequests = DISABLE;
    hadc3.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    if (HAL_ADC_Init(&hadc3) != HAL_OK) {
        Error_Handler();
    }

    ch.Channel = POT_ADC_CHANNEL;
    ch.Rank = 1;
    ch.SamplingTime = ADC_SAMPLETIME_480CYCLES;   /* 10k source impedance */
    if (HAL_ADC_ConfigChannel(&hadc3, &ch) != HAL_OK) {
        Error_Handler();
    }
}

/* cppcheck-suppress constParameterPointer ; signature fixed by the HAL's weak callback */
void HAL_ADC_MspInit(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC3) {
        GPIO_InitTypeDef g = {0};
        __HAL_RCC_ADC3_CLK_ENABLE();
        g.Pin = POT_Pin;
        g.Mode = GPIO_MODE_ANALOG;
        g.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(POT_GPIO_Port, &g);
    }
}

/* ============================ USART1 MSP ================================= */

/* cppcheck-suppress constParameterPointer ; signature fixed by the HAL's weak callback */
void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        GPIO_InitTypeDef g = {0};
        __HAL_RCC_USART1_CLK_ENABLE();
        /* PA9 = TX, PA10 = RX */
        g.Pin = GPIO_PIN_9 | GPIO_PIN_10;
        g.Mode = GPIO_MODE_AF_PP;
        g.Pull = GPIO_PULLUP;
        g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        g.Alternate = GPIO_AF7_USART1;
        HAL_GPIO_Init(GPIOA, &g);
    }
}

/* ============================ Error handler ============================== */

void Error_Handler(void)
{
    __disable_irq();
    /* Fast blink of the on-board red LED: an init step failed */
    for (;;) {
        HAL_GPIO_TogglePin(BOARD_LED_R_GPIO_Port, BOARD_LED_R_Pin);
        for (volatile uint32_t i = 0; i < 2000000; i++) {
        }
    }
}
