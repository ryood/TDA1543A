/* ========================================
 *
 * Copyright YOUR COMPANY, THE YEAR
 * All Rights Reserved
 * UNPUBLISHED, LICENSED SOFTWARE.
 *
 * CONFIDENTIAL AND PROPRIETARY INFORMATION
 * WHICH IS THE PROPERTY OF your company.
 *
 * ========================================
*/
#include "project.h"
#include "wavetable.h"

#define FREQUENCY       (1000u)         // 生成するサイン波の周波数
#define I2SM_CLOCK      (2976000u)      // I2Sコンポーネントのクロック(実測値を指定)
#define SAMPLE_CLOCK    (I2SM_CLOCK/64) 
#define TABLE_SIZE      1024
#define BUFFER_SIZE     4

/* Defines for DMA_0 */
#define DMA_0_BYTES_PER_BURST 1
#define DMA_0_REQUEST_PER_BURST 1
#define DMA_0_SRC_BASE (CYDEV_SRAM_BASE)
#define DMA_0_DST_BASE (CYDEV_PERIPH_BASE)
/* Defines for DMA_1 */
#define DMA_1_BYTES_PER_BURST 1
#define DMA_1_REQUEST_PER_BURST 1
#define DMA_1_SRC_BASE (CYDEV_SRAM_BASE)
#define DMA_1_DST_BASE (CYDEV_PERIPH_BASE)

/* Variable declarations for DMA_0 */
/* Move these variable declarations to the top of the function */
uint8 DMA_0_Chan;
uint8 DMA_0_TD[1];
/* Variable declarations for DMA_1 */
/* Move these variable declarations to the top of the function */
uint8 DMA_1_Chan;
uint8 DMA_1_TD[1];

volatile uint8 waveBuffer_0[BUFFER_SIZE];
volatile uint8 waveBuffer_1[BUFFER_SIZE];

volatile uint32 tuningWord_0;
volatile uint32 tuningWord_1;
volatile uint32 phaseRegister_0 = 0;
volatile uint32 phaseRegister_1 = 0;

void setDDSParameter_0(uint32 frequency)
{
    tuningWord_0 = (((uint64)frequency << 32) / SAMPLE_CLOCK);
}

void setDDSParameter_1(uint16 frequency)
{
    tuningWord_1 = (((uint64)frequency << 32) / SAMPLE_CLOCK);
}

void generateWave_0()
{
    int i, index;
    uint8* p8;
    
    // 波形をバッファに転送
    for (i = 0; i < BUFFER_SIZE; i+=2) {
        phaseRegister_0 += tuningWord_0;
        index = phaseRegister_0 >> 22;
        
        p8 = (uint8 *)(sineTable + index);
        waveBuffer_0[i]   = *(p8 + 1);
        waveBuffer_0[i+1] = *p8;
    }
}

void generateWave_1()
{
    int i, index;
    uint8* p8;
    
    // 波形をバッファに転送
    for (i = 0; i < BUFFER_SIZE; i+=2) {
        phaseRegister_1 += tuningWord_1;
        index = phaseRegister_1 >> 22;
        
        p8 = (uint8 *)(sineTable + index);
        waveBuffer_1[i]   = *(p8 + 1);
        waveBuffer_1[i+1] = *p8;
    }
}

CY_ISR (dma_0_done_handler)
{
    Pin_Check_0_Write(1u);
    generateWave_0();
    Pin_Check_0_Write(0u);
}

CY_ISR (dma_1_done_handler)
{
    Pin_Check_1_Write(1u);
    generateWave_1();
    Pin_Check_1_Write(0u);
}

CY_ISR (i2s_1_tx_handler)
{
    if (I2S_1_ReadTxStatus() & I2S_1_TX_FIFO_UNDERFLOW) {
        // Underflow Alert
    }
}

int main(void)
{
    setDDSParameter_0(FREQUENCY);
    setDDSParameter_1(FREQUENCY);
    generateWave_0();
    generateWave_1();
    
    CyGlobalIntEnable; /* Enable global interrupts. */

    /* Place your initialization/startup code here (e.g. MyInst_Start()) */

    I2S_1_Start();

    /* DMA Configuration for DMA_0 */
    DMA_0_Chan = DMA_0_DmaInitialize(DMA_0_BYTES_PER_BURST, DMA_0_REQUEST_PER_BURST, 
        HI16(DMA_0_SRC_BASE), HI16(DMA_0_DST_BASE));
    DMA_0_TD[0] = CyDmaTdAllocate();
    CyDmaTdSetConfiguration(DMA_0_TD[0], BUFFER_SIZE, DMA_0_TD[0], DMA_0__TD_TERMOUT_EN | TD_INC_SRC_ADR);
    CyDmaTdSetAddress(DMA_0_TD[0], LO16((uint32)waveBuffer_0), LO16((uint32)I2S_1_TX_CH0_F0_PTR));
    CyDmaChSetInitialTd(DMA_0_Chan, DMA_0_TD[0]);
    CyDmaChEnable(DMA_0_Chan, 1);

    /* DMA Configuration for DMA_1 */
    DMA_1_Chan = DMA_1_DmaInitialize(DMA_1_BYTES_PER_BURST, DMA_1_REQUEST_PER_BURST, 
        HI16(DMA_1_SRC_BASE), HI16(DMA_1_DST_BASE));
    DMA_1_TD[0] = CyDmaTdAllocate();
    CyDmaTdSetConfiguration(DMA_1_TD[0], BUFFER_SIZE, DMA_1_TD[0], DMA_1__TD_TERMOUT_EN | TD_INC_SRC_ADR);
    CyDmaTdSetAddress(DMA_1_TD[0], LO16((uint32)waveBuffer_1), LO16((uint32)I2S_1_TX_CH0_F1_PTR));
    CyDmaChSetInitialTd(DMA_1_Chan, DMA_1_TD[0]);
    CyDmaChEnable(DMA_1_Chan, 1);
    
    ISR_DMA_0_Done_StartEx(dma_0_done_handler);
    ISR_DMA_1_Done_StartEx(dma_1_done_handler);
    ISR_I2S_1_TX_StartEx(i2s_1_tx_handler);
    
    while(0u != (I2S_1_ReadTxStatus() & (I2S_1_TX_FIFO_0_NOT_FULL | I2S_1_TX_FIFO_1_NOT_FULL)))
    {
        /* Wait for TxDMA to fill Tx FIFO */
    }
    CyDelay(1);

    I2S_1_EnableTx();
    
    for(;;)
    {
        /* Place your application code here. */
        CyDelay(1);
    }
}

/* [] END OF FILE */
