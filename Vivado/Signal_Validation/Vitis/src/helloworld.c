/******************************************************************************
* FIR ENVELOPE - MONITORIZACIÓN CANAL 1 (Formato Q1.23 corregido)
******************************************************************************/

#include <stdio.h>
#include "xparameters.h"
#include "xil_cache.h"
#include "xaxidma.h"
#include "xil_printf.h"
#include "datos_entrada.h"

// --- CONFIG HW ---
#define DMA_DEV_ID           XPAR_AXIDMA_0_DEVICE_ID
#define WORDS_PER_FRAME      9
#define CHANNELS_OUT         8
#define TOTAL_RAW_SAMPLES    44400
#define NUM_FRAMES           (TOTAL_RAW_SAMPLES / WORDS_PER_FRAME)
#define DMA_TIMEOUT          1000000

// Buffers alineados para DMA
int32_t tx_frame[WORDS_PER_FRAME] __attribute__ ((aligned (64)));
int32_t rx_frame[CHANNELS_OUT]    __attribute__ ((aligned (64)));

XAxiDma AxiDma;

/**
 * Extensión de signo correcta (Q1.23)
 */
static inline int32_t clean_adc_data(u32 raw) {
    return ((int32_t)(raw << 8)) >> 8;
}

int init_dma() {
    XAxiDma_Config *CfgPtr = XAxiDma_LookupConfig(DMA_DEV_ID);
    if (!CfgPtr) return XST_FAILURE;
    if (XAxiDma_CfgInitialize(&AxiDma, CfgPtr) != XST_SUCCESS) return XST_FAILURE;
    XAxiDma_Reset(&AxiDma);
    while (!XAxiDma_ResetIsDone(&AxiDma));
    XAxiDma_IntrDisable(&AxiDma, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DEVICE_TO_DMA);
    XAxiDma_IntrDisable(&AxiDma, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DMA_TO_DEVICE);
    return XST_SUCCESS;
}

int main(void) {
    xil_printf("\r\n=== MONITORIZACION: CANAL 1 (Q1.23) ===\r\n");

    if (init_dma() != XST_SUCCESS) {
        xil_printf("[ERROR] DMA init\r\n");
        return XST_FAILURE;
    }

    xil_printf("FRAME | INPUT(24b) | VALOR Q1.23        | ENVELOPE\r\n");
    xil_printf("---------------------------------------------------\r\n");

    for (int f = 0; f < NUM_FRAMES; f++) {

        // 1. Leer dato raw
        u32 raw_val = datos_txt[f * WORDS_PER_FRAME + 1];

        // 2. Extraer SOLO 24 bits reales del ADC
        u32 adc_24 = raw_val & 0x00FFFFFF;

        // 3. Convertir a Q1.23 correcto (con signo)
        int32_t c1_fixed = clean_adc_data(raw_val);

        // 4. Preparar trama DMA
        tx_frame[0] = (int32_t)datos_txt[f * WORDS_PER_FRAME];
        for (int i = 1; i < WORDS_PER_FRAME; i++) {
            tx_frame[i] = c1_fixed;
        }

        Xil_DCacheFlushRange((UINTPTR)tx_frame, sizeof(tx_frame));
        Xil_DCacheInvalidateRange((UINTPTR)rx_frame, sizeof(rx_frame));

        // DMA
        XAxiDma_SimpleTransfer(&AxiDma, (UINTPTR)rx_frame,
                               CHANNELS_OUT * sizeof(u32),
                               XAXIDMA_DEVICE_TO_DMA);

        XAxiDma_SimpleTransfer(&AxiDma, (UINTPTR)tx_frame,
                               WORDS_PER_FRAME * sizeof(u32),
                               XAXIDMA_DMA_TO_DEVICE);

        u32 timeout = DMA_TIMEOUT;
        while ((XAxiDma_Busy(&AxiDma, XAXIDMA_DMA_TO_DEVICE) ||
                XAxiDma_Busy(&AxiDma, XAXIDMA_DEVICE_TO_DMA)) && timeout--);

        if (timeout == 0) break;

        Xil_DCacheInvalidateRange((UINTPTR)rx_frame, sizeof(rx_frame));

        xil_printf("%5d | 0x%06X   | 0x%08X\r\n",
                   f,
                   adc_24,          // <-- AQUÍ está el dato REAL de 24 bits
                   (u32)rx_frame[0]);
    }

    xil_printf("---------------------------------------------------\r\n");
    xil_printf("[FIN]\r\n");

    while (1);
    return 0;
}
