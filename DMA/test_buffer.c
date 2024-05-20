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
#include "xparameters.h"
#include "xaxidma.h"
#include "xil_io.h"
#include "xil_types.h"
#include "xil_exception.h"
#include "xscugic.h"
#include "xil_cache.h"
#include "sleep.h"
#include "xdebug.h"


#define DMA_DEV_ID		XPAR_AXIDMA_0_DEVICE_ID			// DMA Device ID
#define DMA_BASEADDR 	XPAR_AXI_DMA_0_BASEADDR			// DMA BaseAddr
#define DDR_BASE_ADDR 	XPAR_PS7_DDR_0_S_AXI_BASEADDR	// DDR START ADDRESS
#define MEM_BASE_ADDR	(DDR_BASE_ADDR + 0x1000000)		// MEM START ADDRESS

// REGISTER OFFSETS FOR DMA
// MEMORY TO STREAM REGISTER OFFSETS
#define MM2S_DMACR_OFFSET	0x00
#define MM2S_DMASR_OFFSET 	0x04
#define MM2S_SA_OFFSET 		0x18
#define MM2S_SA_MSB_OFFSET 	0x1c
#define MM2S_LENGTH_OFFSET 	0x28
// STREAM TO MEMORY REGISTER OFFSETS
#define S2MM_DMACR_OFFSET	0x30
#define S2MM_DMASR_OFFSET 	0x34
#define S2MM_DA_OFFSET 		0x48
#define S2MM_DA_MSB_OFFSET 	0x4c
#define S2MM_LENGTH_OFFSET 	0x58

// FLAG BITS INSIDE DMACR REGISTER
#define DMACR_IOC_IRQ_EN 	(1 << 12) // this is IOC_IrqEn bit in DMACR register
#define DMACR_ERR_IRQ_EN 	(1 << 14) // this is Err_IrqEn bit in DMACR register
#define DMACR_RESET 		(1 << 2)  // this is Reset bit in DMACR register
#define DMACR_RS 			 1 		  // this is RS bit in DMACR register

#define DMASR_IOC_IRQ 		(1 << 12) // this is IOC_Irq bit in DMASR register


// TRANSMIT TRANSFER (MEMORY TO STREAM) INTERRUPT ID
#define TX_INTR_ID		XPAR_FABRIC_AXI_DMA_0_MM2S_INTROUT_INTR
// TRANSMIT TRANSFER (MEMORY TO STREAM) BUFFER START ADDRESS
#define TX_BUFFER_BASE	(MEM_BASE_ADDR + 0x00001000)


// RECIEVE TRANSFER (STREAM TO MEMORY) INTERRUPT ID
#define RX_INTR_ID		XPAR_FABRIC_AXI_DMA_0_S2MM_INTROUT_INTR
// RECIEVE TRANSFER (STREAM TO MEMORY) BUFFER START ADDRESS
#define RX_BUFFER_BASE	(MEM_BASE_ADDR + 0x00002000)


// INTERRUPT CONTROLLER DEVICE ID
#define INTC_DEVICE_ID 	XPAR_PS7_SCUGIC_0_DEVICE_ID

//WTF IS THIS
#define RESET_TIMEOUT_COUNTER	10000

// AMOUNT OF BYTES IN A TRANSFER
#define XFER_LENGTH 128*4


//******* FUNCTION DECLARATIONS **********
// Disable Interrupt System
static void Disable_Interrupt_System();
// TRANSMIT TRANSFER (MEMORY TO STREAM) INTERRUPT HANDLER (INTERRUPT SERVICE ROUTINE)
static void Tx_Interrupt_Handler(void *Callback);
//  RECIEVE TRANSFER (STREAM TO MEMORY) INTERRUPT HANDLER (INTERRUPT SERVICE ROUTINE)
static void Rx_Interrupt_Handler(void *Callback);

u32 Setup_Interrupt(u32 DeviceId, Xil_InterruptHandler Handler, u32 interrupt_ID);

void DMA_init_interrupts();
u32 Initialize_System();
u32 DMA_Simple_Write(u32 *TxBufferPtr, u32 max_pkt_len); // my function
u32 DMA_Simple_Read (u32 *RxBufferPtr, u32 max_pkt_len); // my function

volatile int Error;
volatile int tx_intr_done=0;
volatile int rx_intr_done=0;

static XScuGic INTCInst;	// Instance of Interrupt Controller
//static XAxiDma AxiDma;		// Instance of the XAxiDma

u32 *TxBufferPtr = (u32 *)TX_BUFFER_BASE;
u32 *RxBufferPtr = (u32 *)RX_BUFFER_BASE;


int main()
{
  int status;
  int xfer_missmatches =0;
  Xil_DCacheDisable();
  Xil_ICacheDisable();
  init_platform();

  xil_printf("\r\nStarting simulation\r\n");

  //status = Initialize_System();
  //Setup Tx interrupt
  status=Setup_Interrupt(INTC_DEVICE_ID, (Xil_InterruptHandler)Tx_Interrupt_Handler, TX_INTR_ID);
  //Setup Rx interrupt
  status=Setup_Interrupt(INTC_DEVICE_ID, (Xil_InterruptHandler)Rx_Interrupt_Handler, RX_INTR_ID);
  DMA_init_interrupts();
  if (status != XST_SUCCESS) return status;

  //Copy image to TxBuffer
  for(int i=0; i<XFER_LENGTH/4; i++){
    TxBufferPtr[i] = i;
    RxBufferPtr[i] = 0;
  }
  //Start first DMA transaction
  DMA_Simple_Write(TxBufferPtr, XFER_LENGTH); //My function that starts a DMA write transaction
  DMA_Simple_Read (RxBufferPtr, XFER_LENGTH); //My function that starts a DMA read  transaction

  while (!tx_intr_done && !rx_intr_done)
    {
      xil_printf("Waiting... Interrupt Status -> TX:%d | RX:%d\n\r", tx_intr_done, rx_intr_done);
      sleep(1);
    }
  //Change values of TxBuffer
  for(int i=0; i<XFER_LENGTH/4; i++){
    if(	TxBufferPtr[i] != RxBufferPtr[i])
      xfer_missmatches ++;
  }
  if(xfer_missmatches > 0)
    xil_printf("Transfer failed with %d\n\r",xfer_missmatches);
  else
    xil_printf("Transfer successful!!! \n\r");


  sleep(1); //1 sec delay

  cleanup_platform();
  Disable_Interrupt_System();

  return 0;
}

// Interrupt Handler
static void Tx_Interrupt_Handler(void *Callback)
{
  //In every interrupt handler send the next transaction to continue the cycle

  //Read irq status from MM2S_DMASR register
  u32 MM2S_DMASR_reg = Xil_In32(DMA_BASEADDR + MM2S_DMASR_OFFSET);

  //Clear irq status in MM2S_DMASR register
  //(clearing is done by writing 1 on 13. bit in MM2S_DMASR (IOC_Irq)
  Xil_Out32(DMA_BASEADDR + MM2S_DMASR_OFFSET, MM2S_DMASR_reg | DMASR_IOC_IRQ);

  tx_intr_done = 1;
  xil_printf("TX Interrupt Happened!\r\n");
}

// Interrupt Handler
static void Rx_Interrupt_Handler(void *Callback)
{
  //Read irq status from MM2S_DMASR register
  u32 S2MM_DMASR_reg = Xil_In32(DMA_BASEADDR + S2MM_DMASR_OFFSET);

  //Clear irq status in MM2S_DMASR register
  //(clearing is done by writing 1 on 13. bit in MM2S_DMASR (IOC_Irq)
  Xil_Out32(DMA_BASEADDR + S2MM_DMASR_OFFSET, S2MM_DMASR_reg | DMASR_IOC_IRQ);

  rx_intr_done = 1;
  xil_printf("RX Interrupt Happened!\r\n");

}



u32 DMA_Simple_Write(u32 *TxBufferPtr, u32 max_pkt_len) {

  u32 MM2S_DMACR_reg = 0;

  MM2S_DMACR_reg = Xil_In32(DMA_BASEADDR + MM2S_DMACR_OFFSET); // READ from MM2S_DMACR register
  Xil_Out32(DMA_BASEADDR + MM2S_DMACR_OFFSET, MM2S_DMACR_reg | DMACR_RS); // set RS bit in MM2S_DMACR register (this bit starts the DMA)

  Xil_Out32(DMA_BASEADDR + MM2S_SA_OFFSET,  (UINTPTR)TxBufferPtr); // Write into MM2S_SA register the value of TxBufferPtr.
  Xil_Out32(DMA_BASEADDR + MM2S_SA_MSB_OFFSET, 0);				// With this, the DMA knows from where to start.

  Xil_Out32(DMA_BASEADDR + MM2S_LENGTH_OFFSET,  max_pkt_len); // Write into MM2S_LENGTH register. This is the length of a tranaction.

  return 0;
}

u32 DMA_Simple_Read(u32 *RxBufferPtr, u32 max_pkt_len) {

  u32 S2MM_DMACR_reg =0;

  S2MM_DMACR_reg = Xil_In32(DMA_BASEADDR + S2MM_DMACR_OFFSET); // READ from S2MM_DMACR register
  Xil_Out32(DMA_BASEADDR + S2MM_DMACR_OFFSET, S2MM_DMACR_reg | DMACR_RS); // set RS bit in S2MM_DMACR register (this bit starts the DMA)

  Xil_Out32(DMA_BASEADDR + S2MM_DA_OFFSET,  (UINTPTR)RxBufferPtr); // Write into S2MM_SA register the value of TxBufferPtr.
  Xil_Out32(DMA_BASEADDR + S2MM_DA_MSB_OFFSET, 0); 		// With this, the DMA knows from where to start.

  Xil_Out32(DMA_BASEADDR + S2MM_LENGTH_OFFSET,  max_pkt_len); // Write into S2MM_LENGTH register. This is the length of a tranaction.

  return 0;
}

static void Disable_Interrupt_System()
{

  XScuGic_Disconnect(&INTCInst, TX_INTR_ID);
  XScuGic_Disconnect(&INTCInst, RX_INTR_ID);
}

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

void DMA_init_interrupts()
{
  u32 MM2S_DMACR_reg;
  u32 S2MM_DMACR_reg;

  Xil_Out32(DMA_BASEADDR + MM2S_DMACR_OFFSET,  DMACR_RESET); // writing to MM2S_DMACR register
  Xil_Out32(DMA_BASEADDR + S2MM_DMACR_OFFSET,  DMACR_RESET); // writing to S2MM_DMACR register

  /* THIS HERE IS NEEDED TO CONFIGURE DMA*/
  //enable interrupts
  MM2S_DMACR_reg = Xil_In32(DMA_BASEADDR + MM2S_DMACR_OFFSET); // Reading from MM2S_DMACR register inside DMA
  Xil_Out32((DMA_BASEADDR + MM2S_DMACR_OFFSET),  (MM2S_DMACR_reg | DMACR_IOC_IRQ_EN | DMACR_ERR_IRQ_EN)); // writing to MM2S_DMACR register
  S2MM_DMACR_reg = Xil_In32(DMA_BASEADDR + S2MM_DMACR_OFFSET); // Reading from S2MM_DMACR register inside DMA
  Xil_Out32((DMA_BASEADDR + S2MM_DMACR_OFFSET),  (S2MM_DMACR_reg | DMACR_IOC_IRQ_EN | DMACR_ERR_IRQ_EN)); // writing to S2MM_DMACR register
}


