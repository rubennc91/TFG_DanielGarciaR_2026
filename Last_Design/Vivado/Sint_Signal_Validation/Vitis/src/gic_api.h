#ifndef GIC_API_H_
#define GIC_API_H_

#include "xscugic.h"
#include "xil_exception.h"
#include "xparameters.h"

#ifndef XPAR_FABRIC_AXI_DMA_0_S2MM_INTROUT_INTR
#define DMA_RX_IRQ_ID    61U
#else
#define DMA_RX_IRQ_ID    XPAR_FABRIC_AXI_DMA_0_S2MM_INTROUT_INTR
#endif

int setup_pl_irq(XScuGic *intc_instance_ptr);
void dma_rx_irq_handler(void *Callback);

#endif
