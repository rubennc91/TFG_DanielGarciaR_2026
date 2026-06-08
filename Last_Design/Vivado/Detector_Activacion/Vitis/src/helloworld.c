/*****************************************************************************/
/**
* @file main.c
* This file contains a design example using the Spi driver for ADS1298R.
*
******************************************************************************/

/***************************** Include Files *********************************/

#include "xparameters.h"	/* XPAR parameters */
#include "xspi.h"			/* SPI device driver */
#include "xil_exception.h"
#include "xil_printf.h"
#include "ads1298r_api.h"
#include "spi_api.h"
#include "gic_api.h"
#include "dma_api.h"
#include "ads_spi_mux.h"
#include "ads_spi_ctrl_top.h"
#include "xaxidma.h"
#include "xtime_l.h"
#include "xgpio.h"
#include "xgpiops.h"

#include "config.h"

XGpioPs mio_led1;
XGpioPs mio_led2;
#define MIO_52 	52
#define MIO_53	53

// data
volatile uint8_t hasNewData = 0;
ads_data_t *adsData;

uint32_t test_time;

XTime gbl_time_before_test;
XTime *p_gbl_time_before_test = &gbl_time_before_test;
XTime gbl_time_after_test;
XTime *p_gbl_time_after_test = &gbl_time_after_test;
uint32_t dataCount;

unsigned int inicio = 0;
unsigned int fin = 72;
unsigned int columini = 0;
uint8_t contadornueve;
unsigned int filascont;

#define LED_DELAY 	20000000
#define GPIO_LED_ID  XPAR_AXI_GPIO_0_BASEADDR
#define LED_CHANNEL 1
#define LED_ON 0x1

XGpio GpioLED; /* The Instance of the GPIO Driver */


void print_bits(uint8_t v) {
    for (int i = 7; i >= 0; i--) {
        xil_printf("%d", (v >> i) & 1);
    }
}

/****************************************************************************
*
* Main function to call the Spi interrupt example.
* @return	XST_SUCCESS if successful, otherwise XST_FAILURE.
*
******************************************************************************/
int main(void)
{
	int Status;
	uint8_t adsID;
	uint8_t readDATA;

	int Delay;

	xil_printf("[INFO] ADS1298 Example\r\n");

	Xil_DCacheDisable();

    /* Initialize the GPIO driver */
	Status = XGpio_Initialize(&GpioLED, GPIO_LED_ID);
	if (Status != XST_SUCCESS) {
		xil_printf("Gpio Initialization Failed\r\n");
		return XST_FAILURE;
	}

	/* Set the direction for all signals as outputs */
	XGpio_SetDataDirection(&GpioLED, LED_CHANNEL, ~LED_ON);


	xil_printf("[INFO] Configuring SPI MUX: Writing value 0x01 to register 0.\r\n");
	ADS_SPI_MUX_mWriteReg(XPAR_ADS_SPI_MUX_0_S00_AXI_BASEADDR, ADS_SPI_MUX_S00_AXI_SLV_REG0_OFFSET, 0x00);
	usleep(1);
	ADS_SPI_MUX_mWriteReg(XPAR_ADS_SPI_MUX_0_S00_AXI_BASEADDR, ADS_SPI_MUX_S00_AXI_SLV_REG0_OFFSET, 0x01);
	usleep(1);

	// Reset?
	ADS_SPI_CTRL_TOP_mWriteReg(XPAR_ADS_SPI_CTRL_TOP_0_S00_AXI_BASEADDR, ADS_SPI_MUX_S00_AXI_SLV_REG0_OFFSET, 0x00);

	int Test = ADS_begin();
	if (Test == -1){
		for(int k=0; k<10; k++){
			// MIO8 MIO10 DS12 DS23
			XGpio_DiscreteWrite(&GpioLED, LED_CHANNEL, 0x0);
			for (Delay = 0; Delay < LED_DELAY; Delay++);
			XGpio_DiscreteWrite(&GpioLED, LED_CHANNEL, 0x1);
			for (Delay = 0; Delay < LED_DELAY; Delay++);
		}

		return -1;
	}

	// It is not need. Only for example purposes
	xil_printf("[INFO] Example: reset value for all registers without reset command\r\n");
	ADS_setAllRegisterToReset();

	// Read ADS129x ID: 0xD2 for ADS1298R
	Status = SPIreadREGISTER(REGID_REG_ADDR, &adsID);
	if (Status != 0) {
		xil_printf("[ERROR] SPI Configuration Failed\r\n");
	}

	// Check ADS ID
	if (adsID != REGID_ID_ADS1298) {
		xil_printf("[ERROR] Data read at ID_REG (Used: 0x%x): 0x%x \r\n", REGID_ID_ADS1298, adsID);
		return -1;
	}else{
		// Check ADS ID
		xil_printf("[INFO] Data read at ID_REG (Used: 0x%x): 0x%x \r\n", REGID_ID_ADS1298, adsID);
	}

	xil_printf("[INFO] Set sampling read to %d kHz and high resolution mode\r\n", SAMPLING_RATE_TO_KHZ(SAMPLING_RATE) );
	xil_printf("[INFO] Keep in mind that when config1 or resp registers are changed, internal reset is performed. See the datasheet, section Reset\r\n");
	Status = SPIreadREGISTER(CONFIG1_REG_ADDR, &readDATA);
	xil_printf("[INFO] The previous value CONFIG1 register is: 0x%x \r\n", readDATA);

	// By default, ADS12xx is in low-power consumption and with a sample frequency of 250 Hz
	SPIwriteREGISTER(CONFIG1_REG_ADDR, SAMPLING_RATE);
	Status = SPIreadREGISTER(CONFIG1_REG_ADDR, &readDATA);
 	xil_printf("[INFO] The new value CONFIG1 register is (Configured 0x%x): 0x%x \r\n", SAMPLING_RATE, readDATA);

	// Setup of my circuit. In my case, it hadn't external reference,
	xil_printf("[INFO] Enabling internal reference buffer --> set PD_REFBUF to 1\r\n");
	// If you change individual bits with constants B_xx, you must add with the RESERVED_BITS constant value to be sure that you will
	// write the right bits in the reserved bits in the register.
	// Remember to write all desired configuration in a register  simultaneously. When you write a register, you delete all previous values
	SPIwriteREGISTER(CONFIG3_REG_ADDR, CONFIG3_B_PD_REFBUF | CONFIG3_RESERVED_BITS);

	// Wait for internal reference to wake up. See page 15, section Electrical Characteristics in the datasheet,
	usleep(150000); // 150 ms

	xil_printf("[INFO] Selecting internal test signal source for ADS129x.\r\n");
	// Select test signal from chip
	// As example, this 2 methods will keep the SPI open for ADS129x chip for faster configuration. The difference It's not noticeable for humans
	// Be careful when you use this option. Read the documentation before using it.
	SPIwriteREGISTER(CONFIG2_REG_ADDR, CONFIG2_TEST_SOURCE_INTERNAL);
	xil_printf("[INFO] Configuring test signal to square wave at 2 Hz.\r\n");
	// We will use the square signal at 4 Hz
	SPIwriteREGISTER(CONFIG2_REG_ADDR, CONFIG2_TEST_FREQ_4HZ);

	xil_printf("[INFO] Starting channels configuration.\r\n");
	xil_printf("[INFO] Channel 1: gain %d and %s as input.\r\n", GAIN_TO_VALUE(CH1_GAIN), INPUT_TO_STRING(CH1_INPUT));
	ADS_enableChannelAndSetGain(1, CH1_GAIN, CH1_INPUT);

	xil_printf("[INFO] Channel 2: gain %d and %s as input.\r\n", GAIN_TO_VALUE(CH2_GAIN), INPUT_TO_STRING(CH2_INPUT));
	ADS_enableChannelAndSetGain(2, CH2_GAIN, CH2_INPUT);

	xil_printf("[INFO] Channel 3: gain %d and %s as input.\r\n", GAIN_TO_VALUE(CH3_GAIN), INPUT_TO_STRING(CH3_INPUT));
	ADS_enableChannelAndSetGain(3, CH3_GAIN, CH3_INPUT);

	xil_printf("[INFO] Channel 4: gain %d and %s as input.\r\n", GAIN_TO_VALUE(CH4_GAIN), INPUT_TO_STRING(CH4_INPUT));
	ADS_enableChannelAndSetGain(4, CH4_GAIN, CH4_INPUT);

	xil_printf("[INFO] Channel 5: gain %d and %s as input.\r\n", GAIN_TO_VALUE(CH5_GAIN), INPUT_TO_STRING(CH5_INPUT));
	ADS_enableChannelAndSetGain(5, CH5_GAIN, CH5_INPUT);

	xil_printf("[INFO] Channel 6: gain %d and %s as input.\r\n", GAIN_TO_VALUE(CH6_GAIN), INPUT_TO_STRING(CH6_INPUT));
	ADS_enableChannelAndSetGain(6, CH6_GAIN, CH6_INPUT);

	xil_printf("[INFO] Channel 7: gain %d and %s as input.\r\n", GAIN_TO_VALUE(CH7_GAIN), INPUT_TO_STRING(CH7_INPUT));
	ADS_enableChannelAndSetGain(7, CH7_GAIN, CH7_INPUT);

	xil_printf("[INFO] Channel 8: gain %d and %s as input.\r\n", GAIN_TO_VALUE(CH8_GAIN), INPUT_TO_STRING(CH8_INPUT));
	ADS_enableChannelAndSetGain(8, CH8_GAIN, CH8_INPUT);

	xil_printf("[INFO] Sending START command to initiate conversions.\r\n");
	SendSPICommandSTART(0);

	// We need to put ADS in DATA or RDATC mode to receive new data
	// Remember that in RDATAC mode, ADS ignores any SPI command sent if it is not SDATAC command
	xil_printf("[INFO] Set ADS chip in read data (RDATAC) mode.\r\n");
	sendSPICommandRDATAC(0);

	xil_printf("[INFO] Configuring SPI MUX: Writing value 0x03 to register 0.\r\n");
	ADS_SPI_MUX_mWriteReg(XPAR_ADS_SPI_MUX_0_S00_AXI_BASEADDR, ADS_SPI_MUX_S00_AXI_SLV_REG0_OFFSET, 0x03);


	// enable interrupts for DRY
	// enable_dry_irq();
	xil_printf("[INFO] Configuration of DMA.\r\n");
	DMAConfig();

	xil_printf("[INFO] Start loop obtaining the new data.\r\n");

	// Enable signal ss in SPI
	// XSpi_WriteReg(XPAR_AXI_QUAD_SPI_0_BASEADDR, XSP_SSR_OFFSET, 0xfffffffe);

	// 1048576 es 1.0 en formato Q12.20.
	uint32_t umbral_test = 1048576*2;   // Umbral de 2
	xil_printf("[INFO] Ajustando umbrales a 50.0 (0x20)... \r\n");
	for(int ch = 0; ch < 8; ch++){
	    Xil_Out32(XPAR_FIR_FILTER_0_S_AXI_UMBRAL_BASEADDR + 0x20 + (ch * 4), umbral_test);
	}
		int aux_d = 0;
		int cnt = 0;

		xil_printf("[INFO] Waiting for synchronization.\r\n");

		xil_printf("[INFO] Synchronization complete.\r\n");


		uint32_t current_offset = 0;
		xil_printf("[INFO] Synchronization complete.\r\n");

		while(1){
		    if(hasNewData){
		        hasNewData = 0;
		        XGpio_DiscreteWrite(&GpioLED, LED_CHANNEL, aux_d);
		        cnt++;

		        if (aux_d == 1 && cnt == 100){
		        		aux_d = 0;
		        		cnt = 0;
		        }
		        else if(aux_d == 0 && cnt == 10){
		        		aux_d = 1;
		        		cnt = 0;
		        }

		        Xil_DCacheInvalidateRange(OFFSET_MEM_WRITE + current_offset, 4);

		        uint32_t status_word = Xil_In32(OFFSET_MEM_WRITE + current_offset);
		        uint8_t flags = (uint8_t)(status_word & 0xFF);

		        xil_printf("[%06x] ", OFFSET_MEM_WRITE + current_offset);
		        for (int i = 0; i < 8; i++) {
		            // Extraemos el bit i
		            uint8_t bit = (flags >> i) & 1;
		            xil_printf("%d", bit);
		        }
		        xil_printf("\r\n");

		        current_offset += 4;

		        if (current_offset >= (FRAME_COUNT_MAX * 4)) {
		            current_offset = 0;
		        }
		    }
		}

		// You can could the method end() to free GPIO used pins and resources. if you don't need any more de ADS
		// You have to call begin() if you want to use again the ADS
		ADS_end();

		xil_printf("[INFO] Successfully ran ADS Example.\r\n");
		return XST_SUCCESS;
}


