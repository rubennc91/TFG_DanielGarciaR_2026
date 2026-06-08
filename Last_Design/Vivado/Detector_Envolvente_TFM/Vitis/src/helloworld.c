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

int main(void)
{
	int Status;
	uint8_t adsID;
	uint8_t readDATA;

	int Delay;

	xil_printf("[INFO] ADS1298 Example\r\n");

	Xil_DCacheDisable(); // Desactiva el uso del cache para la DDR

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

	ADS_begin();

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

	// HABILITAR RLD SOLO PARA CH2
	SPIwriteREGISTER(0x0D, 0x02);   // RLD_SENSP: solo CH2
	SPIwriteREGISTER(0x0E, 0x02);   // RLD_SENSN: solo CH2

	// Encender amplificador RLD (PD_RLD = 0) y mantener el resto de CONFIG3
	uint8_t cfg3;
	SPIreadREGISTER(0x03, &cfg3);
	cfg3 &= ~0x04;                  // bit 2 = 0 enciende RLD
	SPIwriteREGISTER(0x03, cfg3 | CONFIG3_RESERVED_BITS);
	xil_printf("[INFO] RLD habilitado para CH2.\r\n");


	xil_printf("[INFO] Sending START command to initiate conversions.\r\n");
	SendSPICommandSTART(0);

	// We need to put ADS in DATA or RDATC mode to receive new data
	// Remember that in RDATAC mode, ADS ignores any SPI command sent if it is not SDATAC command
	xil_printf("[INFO] Set ADS chip in read data (RDATAC) mode.\r\n");
	sendSPICommandRDATAC(0);

	xil_printf("[INFO] Configuring SPI MUX: Writing value 0x03 to register 0.\r\n");
	ADS_SPI_MUX_mWriteReg(XPAR_ADS_SPI_MUX_0_S00_AXI_BASEADDR, ADS_SPI_MUX_S00_AXI_SLV_REG0_OFFSET, 0x03);


	xil_printf("[INFO] Configuration of DMA.\r\n");
    DMAConfig();
    xil_printf("[INFO] Start loop obtaining the new data.\r\n");
    xil_printf("[INFO] Waiting for synchronization.\r\n");

    while (!hasNewData);
    hasNewData = 0;

    xil_printf("[INFO] Synchronization complete.\r\n");

    uint32_t print_counter = 0;
    uint32_t current_addr;
    extern volatile uint32_t completed_frame_addr;  // importamos la variable del handler
    int aux_d = 0;
    int cnt = 0;

    while (1) {
        if (hasNewData) {
            hasNewData = 0;
            cnt++;

            if (aux_d == 1 && cnt == 100){
            	aux_d = 0;
            	cnt = 0;
            }
            else if(aux_d == 0 && cnt == 10){
            	aux_d = 1;
            	cnt = 0;
            }

            current_addr = completed_frame_addr;

            Xil_DCacheInvalidateRange(current_addr, 32);

            int32_t q_val = (int32_t)Xil_In32(current_addr + 4); // canal 2 en la palabra 1
            int32_t entera = q_val >> 20;
            uint32_t residuo = (uint32_t)(q_val & 0xFFFFF);
            uint32_t decimales = (uint32_t)(((uint64_t)residuo * 10000) / 1048576);

            xil_printf("%s%d.%04u",
                       (q_val < 0 && entera == 0) ? "-" : "",
                           entera, decimales);
            xil_printf("\r\n");

            print_counter++;
        }
    }

    return 0;
}
