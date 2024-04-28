/******************************************************************************
*
* Copyright (C) 2009 - 2014 Xilinx, Inc.  All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* Use of the Software is limited solely to applications:
* (a) running on a Xilinx device, or
* (b) that interact with a Xilinx device through a bus or interconnect.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* XILINX  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
* OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*
* Except as contained in this notice, the name of the Xilinx shall not be used
* in advertising or otherwise to promote the sale, use or other dealings in
* this Software without prior written authorization from Xilinx.
*
******************************************************************************/

/*
 * helloworld.c: simple test application
 *
 * This application configures UART 16550 to baud rate 9600.
 * PS7 UART (Zynq) is not initialized by this application, since
 * bootrom/bsp configures it to baud rate 115200
 *
 * ------------------------------------------------
 * | UART TYPE   BAUD RATE                        |
 * ------------------------------------------------
 *   uartns550   9600
 *   uartlite    Configurable only in HW design
 *   ps7_uart    115200 (configured by bootrom/bsp)
 */

#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "xparameters.h"
#include "xil_io.h"
#include "xil_exception.h"
#include "xscugic.h"
#include "sleep.h"

// Timer base Addres, can also be found in address editor in vivado
// but this way it is automaticaly generated in xparameters.h file
// when platform project is created
#define TIMER_BASEADDR 		XPAR_AXI_TIMER_0_BASEADDR

// Can also be found in xparameters.h file. Every interrupt has its number
#define TIMER_INTERRUPT_ID 		XPAR_FABRIC_AXI_TIMER_0_INTERRUPT_INTR

// Processor core also has its own device ID which is used to enable
// interrupts
#define INTC_DEVICE_ID 		XPAR_PS7_SCUGIC_0_DEVICE_ID

// REGISTER CONSTANTS used to address correct registers
// inside AXI timer IP
#define XIL_AXI_TIMER_TCSR_OFFSET		0x0
#define XIL_AXI_TIMER_TLR_OFFSET		0x4
#define XIL_AXI_TIMER_TCR_OFFSET		0x8

// Constants used to setup AXI Timer registers
#define XIL_AXI_TIMER_CSR_CASC_MASK		0x00000800
#define XIL_AXI_TIMER_CSR_ENABLE_ALL_MASK	0x00000400
#define XIL_AXI_TIMER_CSR_ENABLE_PWM_MASK	0x00000200
#define XIL_AXI_TIMER_CSR_INT_OCCURED_MASK 	0x00000100
#define XIL_AXI_TIMER_CSR_ENABLE_TMR_MASK 	0x00000080
#define XIL_AXI_TIMER_CSR_ENABLE_INT_MASK 	0x00000040
#define XIL_AXI_TIMER_CSR_LOAD_MASK 		0x00000020
#define XIL_AXI_TIMER_CSR_AUTO_RELOAD_MASK 	0x00000010
#define XIL_AXI_TIMER_CSR_EXT_CAPTURE_MASK 	0x00000008
#define XIL_AXI_TIMER_CSR_EXT_GENERATE_MASK 0x00000004
#define XIL_AXI_TIMER_CSR_DOWN_COUNT_MASK 	0x00000002
#define XIL_AXI_TIMER_CSR_CAPTURE_MODE_MASK 0x00000001

// Funtion Prototypes
u32 Setup_Interrupt(u32 DeviceId, Xil_InterruptHandler Handler, u32 interrupt_ID);
void Timer_Interrupt_Handler();
static void Setup_And_Start_Timer(unsigned int milliseconds);
// Global Variables
volatile int timer_intr_done = 0;
int main()
{
    int status;
    int data;
    
    init_platform();
    status = Setup_Interrupt(INTC_DEVICE_ID,    (Xil_InterruptHandler)Timer_Interrupt_Handler, TIMER_INTERRUPT_ID);

    print("Hello Timer\n\r");

    Setup_And_Start_Timer(5000); // Timer will generate an interrupt after 5 seconds

    while(!timer_intr_done)
    {
    	sleep(1);
       print("Waiting... \n\r");
    }

	// Clear Interrupt Flag
    data = Xil_In32(TIMER_BASEADDR + XIL_AXI_TIMER_TCSR_OFFSET);
    Xil_Out32(TIMER_BASEADDR + XIL_AXI_TIMER_TCSR_OFFSET, (data |  XIL_AXI_TIMER_CSR_INT_OCCURED_MASK));
	// Disable Timer
    data = Xil_In32(TIMER_BASEADDR + XIL_AXI_TIMER_TCSR_OFFSET);
    Xil_Out32(TIMER_BASEADDR + XIL_AXI_TIMER_TCSR_OFFSET, data & ~(XIL_AXI_TIMER_CSR_ENABLE_TMR_MASK));

    print("Successfully ran Hello Timer application");
    cleanup_platform();
    return 0;
}


// Initialize Interrupt Controller
u32 Setup_Interrupt(u32 DeviceId, Xil_InterruptHandler Handler, u32 interrupt_ID)
{
   XScuGic_Config *IntcConfig;
   XScuGic INTCInst;
   int status;
   // Extracts informations about processor core based on its ID, and they are used to setup interrupts
   IntcConfig = XScuGic_LookupConfig(DeviceId);
   // Initializes processor registers using information extracted in the previous step
   status = XScuGic_CfgInitialize(&INTCInst, IntcConfig, IntcConfig->CpuBaseAddress);
   if(status != XST_SUCCESS) return XST_FAILURE;
   status = XScuGic_SelfTest(&INTCInst);
   if (status != XST_SUCCESS) return XST_FAILURE;
   // Connect Timer Handler And Enable Interrupt
   // The processor can have multiple interrupt sources, and we must setup trigger and   priority
   // for the our interrupt. For this we are using interrupt ID.
   XScuGic_SetPriorityTriggerType(&INTCInst, interrupt_ID, 0xA8, 3);
   // Connects out interrupt with the appropriate ISR (Handler)
   status = XScuGic_Connect(&INTCInst, interrupt_ID, Handler, (void *)&INTCInst);
   if(status != XST_SUCCESS) return XST_FAILURE;
   // Enable interrupt for out device
   XScuGic_Enable(&INTCInst, interrupt_ID);
   //Two lines bellow enable exeptions
   Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
(Xil_ExceptionHandler)XScuGic_InterruptHandler,&INTCInst);
   Xil_ExceptionEnable();

   return XST_SUCCESS;
}
// Timer Interrupt Handler
void Timer_Interrupt_Handler()
{
   timer_intr_done = 1;
   return;
}

static void Setup_And_Start_Timer(unsigned int milliseconds)
{
	// Disable Timer Counter
   unsigned int timer_load;
   unsigned int zero = 0;
   unsigned int data = 0;
   // Line bellow is true if timer is working on 100MHz
   timer_load = zero - milliseconds*100000;

   // Disable timer/counter while configuration is in progress
   data = Xil_In32(TIMER_BASEADDR + XIL_AXI_TIMER_TCSR_OFFSET);
   Xil_Out32(TIMER_BASEADDR + XIL_AXI_TIMER_TCSR_OFFSET, (data & ~(XIL_AXI_TIMER_CSR_ENABLE_TMR_MASK)));
   // Set initial value in load register
   Xil_Out32(TIMER_BASEADDR + XIL_AXI_TIMER_TLR_OFFSET, timer_load);
   // Load initial value into counter from load register
   data = Xil_In32(TIMER_BASEADDR + XIL_AXI_TIMER_TCSR_OFFSET);
   Xil_Out32(TIMER_BASEADDR + XIL_AXI_TIMER_TCSR_OFFSET, (data | XIL_AXI_TIMER_CSR_LOAD_MASK));
   // Set LOAD0 bit from the previous step to zero
   data = Xil_In32(TIMER_BASEADDR + XIL_AXI_TIMER_TCSR_OFFSET);
   Xil_Out32(TIMER_BASEADDR + XIL_AXI_TIMER_TCSR_OFFSET, (data & ~(XIL_AXI_TIMER_CSR_LOAD_MASK)));
   // Enable interrupts and autoreload, reset should be zero
   Xil_Out32(TIMER_BASEADDR + XIL_AXI_TIMER_TCSR_OFFSET, (XIL_AXI_TIMER_CSR_ENABLE_INT_MASK | XIL_AXI_TIMER_CSR_AUTO_RELOAD_MASK | XIL_AXI_TIMER_CSR_DOWN_COUNT_MASK));
   // Start Timer by setting enable signal
   data = Xil_In32(TIMER_BASEADDR + XIL_AXI_TIMER_TCSR_OFFSET);
   Xil_Out32(TIMER_BASEADDR + XIL_AXI_TIMER_TCSR_OFFSET, (data | XIL_AXI_TIMER_CSR_ENABLE_TMR_MASK));
}
