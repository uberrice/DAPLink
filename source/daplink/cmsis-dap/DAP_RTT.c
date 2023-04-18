
#include "DAP_config.h"
#include "DAP.h"
#include "DAP_RTT.h"
#include "debug_cm.h"
#include "swd_host.h"
#include <string.h>


const uint8_t rtt_header[] = "SEGGER RTT";
static uint8_t rtt_msg_buf[RTT_MSG_BUF_SIZE+4];
uint8_t rtt_request_buf[6];

static rtt_config_t rtt_config = {
    .scan_start_address = 0x20000000,
    .scan_range = 0x1000,
    .cb_address = 0
};

// prepare request data
    // Format: 
    // 1 byte: DAP Index (ignored for SWD)
    // 2 byte: count (32-bit values)
    // Info byte: DAP Access Port, Read Register; 0b11000000 - 0xC0 (maybe other way around, 0x03, test)
    // info byte has: Transfer Mode (AP), Read/Write Mode (Read), Register (DRW)
uint32_t readBlocks(uint16_t blocks, uint8_t * response) {
    rtt_request_buf[0] = ID_DAP_TransferBlock; // Transfer Block
    rtt_request_buf[1] = 0x00; // DAP Index (SWD ignored)
    rtt_request_buf[2] = blocks; // count
    rtt_request_buf[3] = blocks >> 8; //count
    rtt_request_buf[4] = DAP_TRANSFER_APnDP | DAP_TRANSFER_RnW | AP_DRW; // Info Byte
    rtt_request_buf[5] = 0x00; // Empty for Read
    return DAP_ProcessCommand(rtt_request_buf, response);
}

uint32_t transfer_mem32_block(uint32_t address, uint16_t blocks, uint8_t * response) {
    DAP_TransferAbort = 0U;

    swd_write_ap(AP_CSW, CSW_SIZE32 | CSW_ADDRINC | CSW_DBGSTAT | CSW_HPROT | CSW_MSTRTYPE); // set up CSW (remembers state)
    swd_write_ap(AP_TAR, address); //set address
    uint32_t data;
    for (int16_t i = 0; i < blocks; i++)
    {
        // if( swd_read_ap(AP_DRW, &data) ){
        //     *response++ = (uint8_t) data;
        //     *response++ = (uint8_t)(data >>  8);
        //     *response++ = (uint8_t)(data >> 16);
        //     *response++ = (uint8_t)(data >> 24);
        // }
        readBlocks(blocks, response);
    }
    

    //return readBlocks(blocks, response); // read x amount of blocks
}


// TODO: connect to SWD somehow? Seemingly it's not properly connecting and thus returning an almost empty buffer. Currently getting 'No response from target'
uint32_t RTT_find_control_block(uint32_t start_addr, uint32_t addr_range) {
    if(rtt_config.cb_address != 0){
        return rtt_config.cb_address;
    }
    else{
    unsigned char * result;
        for (uint32_t i = start_addr;
            i < (start_addr + addr_range);
            i += RTT_MSG_BUF_SIZE-RTT_HEADER_LENGTH) //minus header length for overlap (Header between blocks)
        {
            uint32_t transfer_error = transfer_mem32_block(i, RTT_MSG_BUF_SIZE/4, rtt_msg_buf);
            result = strstr(rtt_msg_buf, rtt_header);
            if(result != NULL){ // header found!
                uint32_t rtt_cb_address = i + (result - rtt_msg_buf);
                rtt_config.cb_address = rtt_cb_address;
                return rtt_cb_address;
            }
        }
    }
    return RTT_CB_NOT_FOUND;
}