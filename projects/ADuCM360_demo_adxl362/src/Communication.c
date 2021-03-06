/*!
 *****************************************************************************
 * @file:    Communication.c
 * @brief:
 * @version: $Revision$
 * @date:    $Date$
 *-----------------------------------------------------------------------------
 *
Copyright (c) 2015-2017 Analog Devices, Inc.

All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:
  - Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.
  - Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.
  - Modified versions of the software must be conspicuously marked as such.
  - This software is licensed solely and exclusively for use with processors
    manufactured by or for Analog Devices, Inc.
  - This software may not be combined or merged with other code in any manner
    that would cause the software to become subject to terms and conditions
    which differ from those listed here.
  - Neither the name of Analog Devices, Inc. nor the names of its
    contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.
  - The use of this software may or may not infringe the patent rights of one
    or more patent holders.  This license does not release you from the
    requirement that you obtain separate licenses from these patent holders
    to use this software.

THIS SOFTWARE IS PROVIDED BY ANALOG DEVICES, INC. AND CONTRIBUTORS "AS IS" AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, NON-INFRINGEMENT,
TITLE, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
NO EVENT SHALL ANALOG DEVICES, INC. OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, PUNITIVE OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, DAMAGES ARISING OUT OF CLAIMS OF INTELLECTUAL
PROPERTY RIGHTS INFRINGEMENT; PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/

/***************************** Include Files **********************************/
#include <stdio.h>
#include <stdint.h>

#include "ADuCM360.h"
#include "SpiLib.h"
#include "DioLib.h"

#include "Communication.h"


/************************* Functions Definitions ******************************/

/**
   @brief SPI initialization

   @return none

**/
void SPI_Init(void)
{

   DioPul(pADI_GP0, 0xF8);  /* Disable the internal pull ups on P0[2:0] */

   DioCfg(pADI_GP0, 0x0015);    /* Configure P0[2:0] for SPI1 */

   SpiBaud(pADI_SPI1, 9, SPIDIV_BCRST_DIS);      /* Set the SPI1 clock rate in Master mode to x kHz. */

   SpiCfg(pADI_SPI1, SPICON_MOD_TX1RX1, SPICON_MASEN_EN, SPICON_CON_EN |
          SPICON_RXOF_EN | SPICON_ZEN_EN | SPICON_TIM_TXWR | SPICON_CPOL_LOW |
          SPICON_CPHA_SAMPLELEADING | SPICON_ENABLE_EN); /* Configure SPI1 channel */
}


/**
   @brief Writes a data, a command or a register to the LCD or to ACC via SPI.

   @param ui8address - ACC register address
   @param ui8Data - value to be written
   @enMode ui8Data - write mode

   @return none

**/
void SPI_Write(uint8_t ui8address, uint8_t ui8Data, enWriteData enMode)
{
   if(enMode != SPI_WRITE_REG) {

      DioClr(CSLCD_PORT, CSLCD_PIN);  /* Select LCD */

      if(enMode == SPI_WRITE_DATA) {

         DioSet(A0LCD_PORT, A0LCD_PIN); /* Select to send data */

      } else if(enMode == SPI_WRITE_COMMAND) {

         DioClr(A0LCD_PORT, A0LCD_PIN);   /* Select to send command */
      }

      SpiFifoFlush(pADI_SPI1, SPICON_TFLUSH_EN, SPICON_RFLUSH_EN);     /* Flush Tx and Rx FIFOs */

      SpiTx(pADI_SPI1, ui8Data);

      while ((SpiSta(pADI_SPI1) & SPI1STA_RXFSTA_ONEBYTE) != SPI1STA_RXFSTA_ONEBYTE);   /* Wait until 1 byte is received */

      DioSet(CSLCD_PORT, CSLCD_PIN);   /* Deselect LCD */

   } else {

      DioClr(CSACC_PORT, CSACC_PIN);         /* Select accelerometer */

      SpiFifoFlush(pADI_SPI1, SPICON_TFLUSH_EN, SPICON_RFLUSH_EN);      /* Flush Tx and Rx FIFOs */

      SpiTx(pADI_SPI1, COMM_WRITE);       /* Send write command */

      SpiTx(pADI_SPI1, ui8address);          /* Send register address */

      SpiTx(pADI_SPI1, ui8Data);             /* Send value to be written */

      while ((SpiSta(pADI_SPI1) & SPI1STA_RXFSTA_THREEBYTES) != SPI1STA_RXFSTA_THREEBYTES);        /* Wait until 3 bytes are received */

      DioSet(CSACC_PORT, CSACC_PIN);         /* Deselect accelerometer */
   }

}

/**
   @brief Reads a specified register or two registers address in the accelerometer via SPI.

   @param ui8address - register address
   @param enRegs - register number

   @return reading result

**/
uint16_t SPI_Read(uint8_t ui8address, enRegsNum enRegs)
{

   uint16_t  ui16Result;

   uint8_t ui8value;
   uint16_t ui16valueL;
   uint16_t ui16valueH;

   DioClr(CSACC_PORT, CSACC_PIN);      /* Select accelerometer */

   SpiFifoFlush(pADI_SPI1, SPICON_TFLUSH_EN, SPICON_RFLUSH_EN);       /* Flush Tx and Rx FIFOs */

   SpiTx(pADI_SPI1, COMM_READ);       /* Send read command */

   SpiTx(pADI_SPI1, ui8address);       /* Send register address */

   SpiTx(pADI_SPI1, 0xAA);               /* Send a dummy byte in order to receive the register value */

   if (enRegs == SPI_READ_ONE_REG) {

      while ((SpiSta(pADI_SPI1) & SPI1STA_RXFSTA_THREEBYTES) != SPI1STA_RXFSTA_THREEBYTES);    /* Wait until 3 bytes are received */

      /* Two dummy reads */
      ui8value = SpiRx(pADI_SPI1);
      ui8value = SpiRx(pADI_SPI1);

      /* Read the register value */
      ui8value = SpiRx(pADI_SPI1);

      ui16Result = (uint16_t)ui8value;   /* Set read result*/

   } else {
      SpiTx(pADI_SPI1, 0xAA);

      while ((SpiSta(pADI_SPI1) & SPI1STA_RXFSTA_FOURBYTES) != SPI1STA_RXFSTA_FOURBYTES);      /* Wait until 4 bytes are received */

      /* Two dummy reads */
      ui16valueL = SpiRx(pADI_SPI1);
      ui16valueL = SpiRx(pADI_SPI1);

      /* Read the two register values */
      ui16valueL = SpiRx(pADI_SPI1);
      ui16valueH = SpiRx(pADI_SPI1);

      ui16Result = (uint16_t)((ui16valueH << 8) | ui16valueL); /* Set read result*/
   }

   /* Deselect accelerometer */
   DioSet(CSACC_PORT, CSACC_PIN);

   return ui16Result;
}



