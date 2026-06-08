/******************************************************************************
* MONITOR CANAL 1 – ENVOLVENTE Q12.20
******************************************************************************/

#include <stdio.h>
#include "xparameters.h"
#include "xil_cache.h"
#include "xaxidma.h"
#include "xil_printf.h"
#include "datos_entrada.h"

#define DMA_DEV_ID           XPAR_AXIDMA_0_DEVICE_ID
#define WORDS_PER_FRAME      9
#define CHANNELS_OUT         8
#define TOTAL_RAW_SAMPLES    44400
#define NUM_FRAMES           (TOTAL_RAW_SAMPLES / WORDS_PER_FRAME)  // 4933
#define DMA_TIMEOUT          1000000

int32_t tx_frame[WORDS_PER_FRAME] __attribute__ ((aligned (64)));
int32_t rx_frame[CHANNELS_OUT]    __attribute__ ((aligned (64)));

XAxiDma AxiDma;

// Inicialización del DMA (polling, sin interrupciones)
int init_dma(void) {
    XAxiDma_Config *CfgPtr = XAxiDma_LookupConfig(DMA_DEV_ID);
    if (!CfgPtr) return XST_FAILURE;
    if (XAxiDma_CfgInitialize(&AxiDma, CfgPtr) != XST_SUCCESS) return XST_FAILURE;
    XAxiDma_Reset(&AxiDma);
    while (!XAxiDma_ResetIsDone(&AxiDma));
    XAxiDma_IntrDisable(&AxiDma, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DEVICE_TO_DMA);
    XAxiDma_IntrDisable(&AxiDma, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DMA_TO_DEVICE);
    return XST_SUCCESS;
}

//  MAIN
int main(void) {
    xil_printf("\r\n=== MONITOR CANAL 1 (ENVOLVENTE Q12.20) ===\r\n");

    if (init_dma() != XST_SUCCESS) {
        xil_printf("[ERROR] DMA init\r\n");
        return XST_FAILURE;
    }

    xil_printf("FRAME | ENTRADA CRUDA | ENVOLVENTE (Q12.20) | DECIMAL\r\n");
    xil_printf("-------------------------------------------------------\r\n");

    for (int f = 0; f < NUM_FRAMES; f++) {
        int base = f * WORDS_PER_FRAME;

         for (int i = 0; i < WORDS_PER_FRAME; i++) {
            tx_frame[i] = datos_txt[base + i];
        }

        Xil_DCacheFlushRange((UINTPTR)tx_frame, sizeof(tx_frame));
        Xil_DCacheFlushRange((UINTPTR)rx_frame, sizeof(rx_frame));

        XAxiDma_SimpleTransfer(&AxiDma, (UINTPTR)rx_frame,
                               CHANNELS_OUT * sizeof(int32_t),
                               XAXIDMA_DEVICE_TO_DMA);

        XAxiDma_SimpleTransfer(&AxiDma, (UINTPTR)tx_frame,
                               WORDS_PER_FRAME * sizeof(int32_t),
                               XAXIDMA_DMA_TO_DEVICE);

        uint32_t timeout = DMA_TIMEOUT;
        while ((XAxiDma_Busy(&AxiDma, XAXIDMA_DMA_TO_DEVICE) ||
                XAxiDma_Busy(&AxiDma, XAXIDMA_DEVICE_TO_DMA)) && timeout--);

        if (timeout == 0) {
            xil_printf("[ERROR] DMA timeout en frame %d\r\n", f);
            break;
        }

        Xil_DCacheInvalidateRange((UINTPTR)rx_frame, sizeof(rx_frame));

        int32_t envolvente = rx_frame[0];
        uint32_t raw_ch1 = tx_frame[1];

            int32_t entera = envolvente >> 20;
            uint32_t residuo = (uint32_t)(envolvente & 0xFFFFF);
            uint32_t decimales = (uint32_t)(((uint64_t)residuo * 10000) / 1048576);

            xil_printf("%5d | 0x%08X    | 0x%08X          | %d.%04u\r\n",
                       f, raw_ch1, (uint32_t)envolvente, entera, decimales);

    }

    xil_printf("-------------------------------------------------------\r\n");
    xil_printf("[FIN]\r\n");
    while(1);
    return 0;
}
