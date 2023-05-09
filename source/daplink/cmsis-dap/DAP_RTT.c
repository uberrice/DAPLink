
#include "DAP_config.h"
#include "DAP.h"
#include "DAP_RTT.h"
#include "debug_cm.h"
#include "swd_host.h"
#include <string.h>

uint8_t rtt_request_buf[6];
const uint8_t rtt_header[] = "SEGGER RTT\0\0\0\0\0\0";
static uint8_t rtt_msg_buf[RTT_MSG_BUF_SIZE + 4];
static uint8_t *rtt_transfer_contents = rtt_msg_buf + 4;
static SEGGER_RTT_BUFFER RTT_UpBuffers[RTT_MAX_NUM_UP_BUFFERS];
static SEGGER_RTT_BUFFER RTT_DownBuffers[RTT_MAX_NUM_DOWN_BUFFERS];
static SEGGER_RTT_CB RTT_control_block;

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

uint32_t read_mem32_block(uint32_t address, uint16_t blocks, uint8_t *response)
{
    DAP_TransferAbort = 0U;

    swd_write_ap(AP_CSW, CSW_SIZE32 | CSW_ADDRINC | CSW_DBGSTAT | CSW_HPROT | CSW_MSTRTYPE); // set up CSW (remembers state)
    swd_write_ap(AP_TAR, address);                                                           // set address
    uint32_t data;
    readBlocks(blocks, response);
}

/** Write a single uint32 word to the DAP port at the specified address in memory of the target using SWD
\param address  address to write to
\param data     data to be written to the address
\return         RTT_OK or RTT_FAILURE, depending on if command was successful
 */
uint32_t RTT_swd_write_uint32_word(uint32_t address, uint32_t data) // TODO Test
{
    swd_write_ap(AP_CSW, CSW_SIZE32 | CSW_ADDRINC | CSW_DBGSTAT | CSW_HPROT | CSW_MSTRTYPE); // set up CSW (remembers state)
    swd_write_ap(AP_TAR, address);                                                           // set address
    uint32_t retry = DAP_Data.transfer.retry_count;
    uint32_t response;
    do
    {
        response = SWD_Transfer(DAP_TRANSFER_APnDP | AP_DRW, &data); // write DRW register
    } while ((response == DAP_TRANSFER_WAIT) && retry-- && !DAP_TransferAbort);
    if (response != DAP_TRANSFER_OK)
    {
        return RTT_FAILURE;
    }
    return RTT_OK;
}

/** Read a single uint32 word from the DAP port at the specified address in memory of the target using SWD
\param address  address to read from
\param data     data to be read from the address
\return         RTT_OK or RTT_FAILURE, depending on if command was successful
 */
uint32_t RTT_swd_read_uint32_word(uint32_t address, uint32_t * data) // TODO Test
{
    swd_write_ap(AP_CSW, CSW_SIZE32 | CSW_ADDRINC | CSW_DBGSTAT | CSW_HPROT | CSW_MSTRTYPE); // set up CSW (remembers state)
    swd_write_ap(AP_TAR, address);                                                           // set address
    uint32_t retry = DAP_Data.transfer.retry_count;
    uint32_t response;
    do
    {
        response = SWD_Transfer(DAP_TRANSFER_RnW | DAP_TRANSFER_APnDP | AP_DRW, data); // read DRW register
    } while ((response == DAP_TRANSFER_WAIT) && retry-- && !DAP_TransferAbort);
    if (response != DAP_TRANSFER_OK)
    {
        return RTT_FAILURE;
    }
    return RTT_OK;
}

uint32_t RTT_check_control_block(uint32_t rtt_cb_address)
{
    uint32_t transfer_error = read_mem32_block(rtt_cb_address, 4, rtt_msg_buf);
    for (uint32_t i = 0; i < 16; i++)
    {
        if (rtt_transfer_contents[i] != rtt_header[i])
        {
            return RTT_FAILURE;
        }
    }
    return RTT_OK;
}

uint32_t RTT_read_control_block(uint32_t rtt_cb_address) // TODO test modifications
{
    if (RTT_check_control_block(rtt_cb_address) != RTT_OK)
    {
        return RTT_FAILURE;
    }
    uint32_t transfer_error = read_mem32_block(rtt_cb_address + 16, 2, rtt_msg_buf);

    // Number of Buffers
    RTT_control_block.MaxNumUpBuffers = deserialize_uint32(rtt_transfer_contents);
    RTT_control_block.MaxNumDownBuffers = deserialize_uint32(rtt_transfer_contents + 4);

    // Initialize Buffers
    for (uint32_t i = 0; i < RTT_control_block.MaxNumUpBuffers; i++)
    {
        RTT_UpBuffers[i].address = rtt_cb_address + 24 + (i * 24);
        transfer_error = read_mem32_block(RTT_UpBuffers[i].address, 6, rtt_msg_buf);
        RTT_UpBuffers[i].sName = (char *)deserialize_uint32(rtt_transfer_contents);
        RTT_UpBuffers[i].pBuffer = (char *)deserialize_uint32(rtt_transfer_contents + 4);
        RTT_UpBuffers[i].SizeOfBuffer = deserialize_uint32(rtt_transfer_contents + 8);
        RTT_UpBuffers[i].WrOff = deserialize_uint32(rtt_transfer_contents + 12);
        RTT_UpBuffers[i].RdOff = deserialize_uint32(rtt_transfer_contents + 16);
        RTT_UpBuffers[i].Flags = deserialize_uint32(rtt_transfer_contents + 20);
    }
    for (uint32_t i = 0; i < RTT_control_block.MaxNumDownBuffers; i++)
    {
        RTT_DownBuffers[i].address = rtt_cb_address + 24 + (24 * RTT_control_block.MaxNumUpBuffers) + (i * 24);
        transfer_error = read_mem32_block(RTT_DownBuffers[i].address, 6, rtt_msg_buf);
        RTT_DownBuffers[i].sName = (char *)deserialize_uint32(rtt_transfer_contents);
        RTT_DownBuffers[i].pBuffer = (char *)deserialize_uint32(rtt_transfer_contents + 4);
        RTT_DownBuffers[i].SizeOfBuffer = deserialize_uint32(rtt_transfer_contents + 8);
        RTT_DownBuffers[i].WrOff = deserialize_uint32(rtt_transfer_contents + 12);
        RTT_DownBuffers[i].RdOff = deserialize_uint32(rtt_transfer_contents + 16);
        RTT_DownBuffers[i].Flags = deserialize_uint32(rtt_transfer_contents + 20);
    }
    return RTT_OK;
}

uint32_t test_RTT_readBuf1(void)
{
    uint32_t transfer_error = read_mem32_block((uint32_t)RTT_UpBuffers[0].pBuffer, RTT_UpBuffers[0].SizeOfBuffer / 4, rtt_msg_buf);
}

uint32_t RTT_read_up_buffers(void)
{
    for(uint32_t i = 0; i < RTT_control_block.MaxNumUpBuffers; i++)
    {
        if(RTT_UpBuffers[i].address != 0)
        {
            //update pointers
            uint32_t transfer_error = read_mem32_block(RTT_UpBuffers[i].address + RTT_BUFFER_POSITION_OFFSET, 2, rtt_msg_buf);
            RTT_UpBuffers[i].WrOff = deserialize_uint32(rtt_transfer_contents);
            RTT_UpBuffers[i].RdOff = deserialize_uint32(rtt_transfer_contents+4);

            if(RTT_UpBuffers[i].WrOff != RTT_UpBuffers[i].RdOff){ // new data
                if(RTT_UpBuffers[i].WrOff > RTT_UpBuffers[i].RdOff)
                {   //no loop in ringbuffer
                    // TODO read with read_mem32 to buffer for up buffers
                    read_mem32_block((uint32_t)RTT_UpBuffers[i].pBuffer + RTT_UpBuffers[i].RdOff, ((RTT_UpBuffers[i].WrOff - RTT_UpBuffers[i].RdOff) / 4) + 1, rtt_msg_buf); //FIXME - This reads too much data, 1-4 bytes. Remove manually? Other function?
                }
                else
                {   //loop in ringbuffer
                    // Todo read with read_mem32 to buffer for up memories; once to end of buffer (RTT_UpBuffers[i].SizeOfBuffer), once from start to write pointer
                }
            }

        }
    }

}

uint32_t RTT_find_cb_in_buf(uint8_t *buf)
{
    uint8_t flag = 1;
    for (uint32_t i = 0; i < RTT_MSG_BUF_SIZE - 16; i++)
    {
        flag = 1;
        for (uint16_t j = 0; j < 16; j++)
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
    if (RTT_control_block.cb_address != 0) // check if control block exists at set address
    {
        if (RTT_check_control_block(RTT_control_block.cb_address) == RTT_OK)
            return RTT_control_block.cb_address;
    }
    uint32_t rtt_cb_position = RTT_CB_NOT_FOUND;
    for (uint32_t i = start_addr;
         i < (start_addr + addr_range);
         i += (RTT_MSG_BUF_SIZE - RTT_HEADER_LENGTH)) // minus header length for overlap (Header between blocks)
    {
        uint32_t transfer_error = read_mem32_block(i, RTT_MSG_BUF_SIZE / 4, rtt_msg_buf);

        rtt_cb_position = RTT_find_cb_in_buf(rtt_transfer_contents);
        if (rtt_cb_position != RTT_CB_NOT_FOUND)
        { // header found!
            uint32_t rtt_cb_address = i + rtt_cb_position;
            RTT_control_block.cb_address = rtt_cb_address;
            RTT_read_control_block(RTT_control_block.cb_address);
            test_RTT_readBuf1(); // TODO remove temp
            return rtt_cb_address;
        }
    }

    return RTT_CB_NOT_FOUND;
}