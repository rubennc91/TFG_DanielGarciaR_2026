#include "xparameters.h"
#include "dma_api.h"

unsigned int frame_count = 0;
volatile uint32_t completed_frame_addr = OFFSET_MEM_WRITE; // para sincronizar con main

XScuGic intc;
static XScuGic_Config *GicConfig;
XAxiDma AxiDma;
static XAxiDma_Config *DmaConfig;

extern volatile uint8_t hasNewData;
#define FIR_BYTES_PER_FRAME 32

void dmaIRQ_Handler(void *CallbackRef)
{
    uint32_t status = Xil_In32(XPAR_AXI_DMA_0_BASEADDR + OFFSET_S2MM_DMASR);

    // Si hay error, limpiar y resetear el DMA
    if (status & 0x10) {  // bit 4: DMA Internal Error
        // Escribir 1 en el bit de error para limpiarlo
        Xil_Out32(XPAR_AXI_DMA_0_BASEADDR + OFFSET_S2MM_DMASR, 0x10);
        // Resetear S2MM
        uint32_t dmacr = Xil_In32(XPAR_AXI_DMA_0_BASEADDR + OFFSET_S2MM_DMACR);
        Xil_Out32(XPAR_AXI_DMA_0_BASEADDR + OFFSET_S2MM_DMACR, dmacr | 0x4);   // Reset
        usleep(100);
        Xil_Out32(XPAR_AXI_DMA_0_BASEADDR + OFFSET_S2MM_DMACR, dmacr & ~0x4);  // Release reset
        usleep(100);
        // Reconfigurar control (run + IOC)
        Xil_Out32(XPAR_AXI_DMA_0_BASEADDR + OFFSET_S2MM_DMACR, 0x1001);
        // Programar la misma dirección (frame_count no ha cambiado)
        Xil_Out32(XPAR_AXI_DMA_0_BASEADDR + OFFSET_S2MMDA, OFFSET_MEM_WRITE + frame_count * 32);
        Xil_Out32(XPAR_AXI_DMA_0_BASEADDR + OFFSET_S2MM_LENGTH, FIR_BYTES_PER_FRAME);
        // Nota: no se activa hasNewData aquí, pues es un error; el DMA se rearma a la espera de datos válidos
        return;
    }

    // Si interrupción normal (IOC)
    if (status & 0x1000) {  // bit 12: IOC
        // Limpiar IOC escribiendo 1 en ese bit
        Xil_Out32(XPAR_AXI_DMA_0_BASEADDR + OFFSET_S2MM_DMASR, 0x1000);

        // Guardar la dirección de la trama que ACABA de terminar
        completed_frame_addr = OFFSET_MEM_WRITE + frame_count * 32;

        // Avanzar frame_count circularmente
        frame_count++;
        if (frame_count >= FRAME_COUNT_MAX) {
            frame_count = 0;
        }

        // Programar la siguiente transferencia
        uint32_t next_addr = OFFSET_MEM_WRITE + frame_count * 32;
        Xil_Out32(XPAR_AXI_DMA_0_BASEADDR + OFFSET_S2MMDA, next_addr);
        Xil_Out32(XPAR_AXI_DMA_0_BASEADDR + OFFSET_S2MM_LENGTH, FIR_BYTES_PER_FRAME);

        hasNewData = 1;
    }
}

int SetupInterruptSystem(XScuGic *xScuGicInstancePtr)
{
    Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT, (Xil_ExceptionHandler)XScuGic_InterruptHandler, xScuGicInstancePtr);
    Xil_ExceptionEnable();
    return XST_SUCCESS;
}

void DMAConfig(void)
{
    // Reset S2MM
    uint32_t dmacr = Xil_In32(XPAR_AXI_DMA_0_BASEADDR + OFFSET_S2MM_DMACR);
    Xil_Out32(XPAR_AXI_DMA_0_BASEADDR + OFFSET_S2MM_DMACR, dmacr | 0x4);
    usleep(100);
    Xil_Out32(XPAR_AXI_DMA_0_BASEADDR + OFFSET_S2MM_DMACR, dmacr & ~0x4);
    usleep(100);

    // Habilitar DMA (run, IOC)
    Xil_Out32(XPAR_AXI_DMA_0_BASEADDR + OFFSET_S2MM_DMACR, 0x1001);
    Xil_Out32(XPAR_AXI_DMA_0_BASEADDR + OFFSET_S2MM_DMASR, 0x1000); // Limpiar IOC residual

    // Interrupt system
    GicConfig = XScuGic_LookupConfig(XPAR_PS7_SCUGIC_0_DEVICE_ID);
    XScuGic_CfgInitialize(&intc, GicConfig, GicConfig->CpuBaseAddress);
    SetupInterruptSystem(&intc);
    XScuGic_Connect(&intc, XPAR_FABRIC_AXI_DMA_0_S2MM_INTROUT_INTR, (Xil_ExceptionHandler)dmaIRQ_Handler, NULL);
    XScuGic_Enable(&intc, XPAR_FABRIC_AXI_DMA_0_S2MM_INTROUT_INTR);

    // Primera transferencia
    frame_count = 0;
    Xil_Out32(XPAR_AXI_DMA_0_BASEADDR + OFFSET_S2MMDA, OFFSET_MEM_WRITE);
    Xil_Out32(XPAR_AXI_DMA_0_BASEADDR + OFFSET_S2MM_LENGTH, FIR_BYTES_PER_FRAME);
}
