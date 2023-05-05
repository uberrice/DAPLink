
#include "DAP_config.h"
#include "DAP.h"
#include "DAP_RTT.h"
#include "debug_cm.h"
#include "swd_host.h"
#include <string.h>

uint8_t rtt_request_buf[6];
const uint8_t rtt_header[] = "SEGGER RTT";
static uint8_t rtt_msg_buf[RTT_MSG_BUF_SIZE + 4];
static uint8_t *rtt_transfer_contents = rtt_msg_buf + 4;
static SEGGER_RTT_BUFFER RTT_UpBuffers[RTT_MAX_NUM_UP_BUFFERS];
static SEGGER_RTT_BUFFER RTT_DownBuffers[RTT_MAX_NUM_DOWN_BUFFERS];

static rtt_config_t rtt_config = {
    .scan_start_address = 0x10000000,
    .scan_range = 0x1000,
    .cb_address = 0};

uint32_t deserialize_uint32(uint8_t *buf)
{
    uint32_t res = 0;

    res |= buf[3] << 24;
    res |= buf[2] << 16;
    res |= buf[1] << 8;
    res |= buf[0];
    return res;
}

// prepare request data
// Format:
// 1 byte: DAP Index (ignored for SWD)
// 2 byte: count (32-bit values)
// Info byte: DAP Access Port, Read Register; 0b11000000 - 0xC0 (maybe other way around, 0x03, test)
// info byte has: Transfer Mode (AP), Read/Write Mode (Read), Register (DRW)
uint32_t readBlocks(uint16_t blocks, uint8_t *response)
{
    rtt_request_buf[0] = ID_DAP_TransferBlock;                           // Transfer Block
    rtt_request_buf[1] = 0x00;                                           // DAP Index (SWD ignored)
    rtt_request_buf[2] = blocks;                                         // count
    rtt_request_buf[3] = blocks >> 8;                                    // count
    rtt_request_buf[4] = DAP_TRANSFER_APnDP | DAP_TRANSFER_RnW | AP_DRW; // Info Byte
    rtt_request_buf[5] = 0x00;                                           // Empty for Read
    return DAP_ProcessCommand(rtt_request_buf, response);
}

uint32_t transfer_mem32_block(uint32_t address, uint16_t blocks, uint8_t *response)
{
    DAP_TransferAbort = 0U;

    swd_write_ap(AP_CSW, CSW_SIZE32 | CSW_ADDRINC | CSW_DBGSTAT | CSW_HPROT | CSW_MSTRTYPE); // set up CSW (remembers state)
    swd_write_ap(AP_TAR, address);                                                           // set address
    uint32_t data;
    readBlocks(blocks, response);
}

uint32_t RTT_read_control_block(uint32_t cb_addr)
{
    uint32_t transfer_error = transfer_mem32_block(cb_addr, RTT_MSG_BUF_SIZE / 4, rtt_msg_buf);
    uint8_t *buf_ptr = rtt_transfer_contents;
    buf_ptr += 16; // SEGGER RTT\0\0\0\0\0\0

    // Number of Buffers
    uint32_t num_up_buf = deserialize_uint32(buf_ptr);
    buf_ptr += 4;
    uint32_t num_down_buf = deserialize_uint32(buf_ptr);
    buf_ptr += 4;

    // Initialize Buffers
    for (uint32_t i = 0; i < num_up_buf; i++)
    {
        RTT_UpBuffers[i].sName = (char *)deserialize_uint32(buf_ptr);
        buf_ptr += 4;
        RTT_UpBuffers[i].pBuffer = (char *)deserialize_uint32(buf_ptr);
        buf_ptr += 4;
        RTT_UpBuffers[i].SizeOfBuffer = deserialize_uint32(buf_ptr);
        buf_ptr += 4;
        RTT_UpBuffers[i].WrOff = deserialize_uint32(buf_ptr);
        buf_ptr += 4;
        RTT_UpBuffers[i].RdOff = deserialize_uint32(buf_ptr);
        buf_ptr += 4;
        RTT_UpBuffers[i].Flags = deserialize_uint32(buf_ptr);
        buf_ptr += 4;
    }
    for (uint32_t i = 0; i < num_down_buf; i++)
    {
        RTT_DownBuffers[i].sName = (char *)deserialize_uint32(buf_ptr);
        buf_ptr += 4;
        RTT_DownBuffers[i].pBuffer = (char *)deserialize_uint32(buf_ptr);
        buf_ptr += 4;
        RTT_DownBuffers[i].SizeOfBuffer = deserialize_uint32(buf_ptr);
        buf_ptr += 4;
        RTT_DownBuffers[i].WrOff = deserialize_uint32(buf_ptr);
        buf_ptr += 4;
        RTT_DownBuffers[i].RdOff = deserialize_uint32(buf_ptr);
        buf_ptr += 4;
        RTT_DownBuffers[i].Flags = deserialize_uint32(buf_ptr);
        buf_ptr += 4;
    }

    return 1;
}

uint32_t test_RTT_readBuf1(void)
{
    uint32_t transfer_error = transfer_mem32_block((uint32_t)RTT_UpBuffers[0].pBuffer, RTT_UpBuffers[0].SizeOfBuffer / 4, rtt_msg_buf);
}

uint32_t RTT_find_cb_in_buf(uint8_t *buf)
{
    uint8_t flag = 1;
    for (uint32_t i = 0; i < RTT_MSG_BUF_SIZE - strlen(rtt_header); i++)
    {
        flag = 1;
        for (uint16_t j = 0; j < strlen(rtt_header); j++)
        {
            if (buf[i + j] != rtt_header[j])
            {
                flag = 0;
                break;
            }
        }
        if (flag == 1)
        {
            return i;
        }
    }
    return RTT_CB_NOT_FOUND;
}
uint32_t RTT_find_control_block(uint32_t start_addr, uint32_t addr_range)
{
    if (0)
    {
        return rtt_config.cb_address;
    }
    else
    {
        uint32_t rtt_cb_position = RTT_CB_NOT_FOUND;
        for (uint32_t i = start_addr;
             i < (start_addr + addr_range);
             i += (RTT_MSG_BUF_SIZE - RTT_HEADER_LENGTH)) // minus header length for overlap (Header between blocks)
        {
            uint32_t transfer_error = transfer_mem32_block(i, RTT_MSG_BUF_SIZE / 4, rtt_msg_buf);

            rtt_cb_position = RTT_find_cb_in_buf(rtt_transfer_contents);
            if (rtt_cb_position != RTT_CB_NOT_FOUND)
            { // header found!
                uint32_t rtt_cb_address = i + rtt_cb_position;
                rtt_config.cb_address = rtt_cb_address;
                RTT_read_control_block(rtt_config.cb_address);
                test_RTT_readBuf1();
                return rtt_cb_address;
            }
        }
    }
    return RTT_CB_NOT_FOUND;
}