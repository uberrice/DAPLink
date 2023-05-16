/*
 * Copyright (c) 2013-2020 Arm Limited. All rights reserved.
 * Copyright 2019, Cypress Semiconductor Corporation
 * or a subsidiary of Cypress Semiconductor Corporation.
 * Copyright (c) 2021 Chris Reed
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * ----------------------------------------------------------------------
 *
 * $Date:        20. May 2015
 * $Revision:    V1.10
 *
 * Project:      CMSIS-DAP Source
 * Title:        DAP_vendor.c CMSIS-DAP Vendor Commands
 *
 *---------------------------------------------------------------------------*/

#include "rl_usb.h"
#include "DAP_config.h"
#include "DAP.h"
#include "info.h"
#include "daplink.h"
#include DAPLINK_MAIN_HEADER
#include "uart.h"
#include "settings.h"
#include "target_family.h"
#include "flash_manager.h"
#include <string.h>
#include "daplink_vendor_commands.h"
#include "mat_vendor_commands.h"

#include "lpadc.h"
#include "DAP_PC_reader.h"

#ifdef DRAG_N_DROP_SUPPORT
#include "file_stream.h"
#endif

//**************************************************************************************************
/**
\defgroup DAP_Vendor_Adapt_gr Adapt Vendor Commands
\ingroup DAP_Vendor_gr
@{

The file DAP_vendor.c provides template source code for extension of a Debug Unit with
Vendor Commands. Copy this file to the project folder of the Debug Unit and add the
file to the MDK-ARM project under the file group Configuration.
*/

/** Process DAP Vendor Command and prepare Response Data
\param request   pointer to request data
\param response  pointer to response data
\return          number of bytes in response (lower 16 bits)
                 number of bytes in request (upper 16 bits)
*/

const char *testStr = "1234567890";

uint32_t DAP_ProcessVendorCommand(const uint8_t *request, uint8_t *response) {
  uint32_t num = (1U << 16) | 1U;

  *response++ = *request;        // copy Command ID

  switch (*request++) {          // first byte in request is Command ID
    case ID_DAP_GetUniqueID: {
        const char *id_str = info_get_unique_id();
        uint8_t len = strlen(id_str);
        *response++ = len;
        memcpy(response, id_str, len);
        num += (len + 1); // increment response count by ID length + length byte
        break;
    }
    case ID_DAP_UART_GetLineCoding: {
        // get line coding
        int32_t read_len = sizeof(CDC_LINE_CODING);
        CDC_LINE_CODING cdc_line_coding;
        USBD_CDC_ACM_PortGetLineCoding(&cdc_line_coding);
        memcpy(response, &cdc_line_coding, read_len);
        num += (read_len + 1);
        break;
    }
    case ID_DAP_UART_SetConfiguration: {
        // set uart configuration
        CDC_LINE_CODING cdc_line_coding;
        USBD_CDC_ACM_PortGetLineCoding(&cdc_line_coding);
        //set BaudRate
        uint32_t baud_rate = 0;
        memcpy(&baud_rate, request, sizeof(uint32_t));
        cdc_line_coding.dwDTERate = baud_rate;
        USBD_CDC_ACM_PortSetLineCoding(&cdc_line_coding);
        USBD_CDC_ACM_SendBreak(0);
        *response = 1;
        num += (sizeof(uint32_t) << 16) | 1;
        break;
    }
    case ID_DAP_UART_Read:  {
        // uart read
        int32_t read_len = 62;
        read_len = uart_read_data(response + 1, read_len);
        if (read_len) {
            main_blink_cdc_led(MAIN_LED_FLASH);
        }
        response[0] = read_len;
        // increment request and response count
        num += (read_len + 1);
        break;
    }
    case ID_DAP_UART_Write:  {
        // uart write
        int32_t write_len = *request;
        request++;
        uart_write_data((uint8_t *)request, write_len);
        main_blink_cdc_led(MAIN_LED_FLASH);
        *response = 1;
        num += ((write_len + 1) << 16) | 1;
        break;
    }
    case ID_DAP_Vendor5:  break;
    case ID_DAP_Vendor6:  break;
    case ID_DAP_Vendor7:  break;
    case ID_DAP_SetUSBTestMode: {
        *response = 1;
        if (0 == *request) {
            main_usb_set_test_mode(false);
        } else if (1 == *request) {
            main_usb_set_test_mode(true);
        } else {
            *response = 0;
        }
        num += (1U << 16) | 1U; // increment request and response count each by 1
        break;
    }
    case ID_DAP_ResetTargetIfNoAutoReset: {
        // reset target
        *response = 1;
        if (!config_get_auto_rst()) {
            target_set_state(RESET_RUN);
        }
        num += 1;
        break;
    }
#ifdef DRAG_N_DROP_SUPPORT
    case ID_DAP_MSD_Open: {
        // open mass storage device stream
        *response = stream_open((stream_type_t)(*request));
        num += (1 << 16) | 1;
        break;
    }
    case ID_DAP_MSD_Close: {
        // close mass storage device stream
        *response = stream_close();
        num += 1;
        break;
    }
    case ID_DAP_MSD_Write: {
        // write to mass storage device
        uint32_t write_len = *request;
        request++;
        main_blink_msc_led(MAIN_LED_FLASH);
        *response = stream_write((uint8_t *)request, write_len);
        num += ((write_len + 1) << 16) | 1;
        break;
    }
#endif
    case ID_DAP_SelectEraseMode: {
        // switching between chip erase and page erase
        //              COMMAND(OUT Packet)
        //              BYTE 0 1000 1110 0x8D
        //              BYTE 1 Desired Mode:
        //                                              0x00 - Chip Erase
        //                                              nonzero - Page Erase
        //              RESPONSE(IN Packet)
        //              BYTE 0
        //                                              0x00 - OK
        *response = DAP_OK;
        if (0x00U == *request) {
            flash_manager_set_page_erase(false);
        } else {
            flash_manager_set_page_erase(true);
        }
        num += (1U << 16) | 1U; // increment request and response count each by 1
        break;
    }
    case ID_DAP_Vendor14: break;
    case ID_DAP_Vendor15: break;
    case ID_DAP_Vendor16: break;
    case ID_DAP_Vendor17: break;
    case ID_DAP_Vendor18: break;
    case ID_DAP_Vendor19: break;
    case ID_DAP_Vendor20: break;
    case ID_DAP_Vendor21: break;
    case ID_DAP_Vendor22: break;
    case ID_DAP_Vendor23: break;
    case ID_DAP_Vendor24: break;
    case ID_DAP_Vendor25: break;
    case ID_DAP_Vendor26: break;
    case ID_DAP_Vendor27: break;
    case ID_DAP_Vendor28: { // read LPADC and PC, and return them both
        uint16_t current = LPADC_polling_current_read();
        uint32_t pc = PC_programcounter_read_and_clear();
        *response++ = (uint8_t) current>>8;
        *response++ = (uint8_t) current;
        *response++ = (uint8_t) pc>>24;
        *response++ = (uint8_t) pc>>16;
        *response++ = (uint8_t) pc>>8;
        *response++ = (uint8_t) pc;
        num += 6; //6 response bytes
        break;
    }
    case ID_DAP_Vendor29: { // VENDOR 29: Current Measurement test
        // can set resistor state and current mode from request bytes, then measures once and responds
        uint8_t resstate = *request++;
        uint8_t currstate = *request;
        set_TargetPowerDisconnect(true);
        if(currstate == 0)
        {
            set_LPADC0_currentMode(LPADC_current_low_sens);
        }
        else
        {
            set_LPADC0_currentMode(LPADC_current_high_sens);
        }
        set_CalibrationResistorState(resstate);
        uint16_t res = LPADC_polling_current_read();
        *response++ = res>>8;
        *response++ = (res & 0xFF);
        num+=2;
        break;
    }
    case ID_DAP_Vendor30: { //find control block
        uint32_t found = PC_find_control_block(0x10000000,0x4000, 0x100);
        *response++ = 4;
        *response++ = found>>24;
        *response++ = found>>16;
        *response++ = found>>8;
        *response++ = found;
        num += 5; // increment response count by ID length + length byte
        break;
    }
    case ID_DAP_TestCommand: {
        uint16_t len = strlen(testStr);
        *response++ = len;
        for (uint16_t i = 0; i < 810; i++)
        {
            memcpy(response, testStr, len);
            response+=(len);
            num+=len;
        }
        num++; // length byte
        break;
    }
    default: break;
  }

  return (num);
}

///@}
