
#include "DAP_config.h"
#include "DAP.h"
#include "DAP_PC_reader.h"
#include "debug_cm.h"
#include "swd_host.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const uint8_t pc_header[8] = "PC loc\0\0";

uint32_t PC_swd_read_and_clear(uint32_t address, uint32_t * data){
    swd_write_ap(AP_CSW, CSW_SIZE32 | CSW_DBGSTAT | CSW_HPROT | CSW_MSTRTYPE);                  // set up CSW (remembers state)
    swd_write_ap(AP_TAR, address);                                                              // set address
    swd_read_ap(AP_DRW, data);
    swd_write_ap(AP_DRW, PC_BUFFER_EMPTY);
}

uint32_t PC_swd_read_words(uint32_t address, uint32_t words, uint8_t * data)
{
    swd_write_ap(AP_CSW, CSW_SIZE32 | CSW_DBGSTAT | CSW_HPROT | CSW_MSTRTYPE | CSW_ADDRINC);    // set up CSW (remembers state)
    swd_write_ap(AP_TAR, address);                                                              // set address
    swd_transfer_retry(DAP_TRANSFER_RnW | DAP_TRANSFER_APnDP | AP_DRW, NULL); // first read is a dummy read
    while(words--)
    {
        uint32_t response;
        swd_transfer_retry(DAP_TRANSFER_RnW | DAP_TRANSFER_APnDP | AP_DRW, &response);
        *data++ = (uint8_t) response;
        *data++ = (uint8_t)(response >>  8);
        *data++ = (uint8_t)(response >> 16);
        *data++ = (uint8_t)(response >> 24);
    }
}

uint32_t PC_find_cb_in_buf(uint8_t *buf, uint32_t buf_size)
{
    uint8_t flag = 1;
    for (uint32_t i = 0; i < buf_size - sizeof(pc_header); i++)
    {
        flag = 1;
        for (uint16_t j = 0; j < sizeof(pc_header); j++)
        {
            if (buf[i + j] != pc_header[j])
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
    return PC_CB_NOT_FOUND;
}
/// @brief Finds the control block
/// @param start_addr       Address in memory of the target CPU to start searching
/// @param addr_range       Range to search (offset)
/// @param stride           Number of bytes to check at once
/// @return                 Address of control block or PC_CB_NOT_FOUND
uint32_t PC_find_control_block(uint32_t start_addr, uint32_t addr_range, uint32_t stride)
{
    uint32_t pc_cb_offset = PC_CB_NOT_FOUND;
    uint32_t pc_cb_address = PC_CB_NOT_FOUND;

    stride -= (stride % 4); // read function reads in 32-bit blocks, so reads have to happen in 32-bit increments

    uint8_t * buf = (uint8_t*) malloc(stride * sizeof(uint8_t));
    for (uint32_t i = start_addr;
         i < (start_addr + addr_range);
         i += (stride - sizeof(pc_header))) // minus header length for overlap (to ensure header contained in one block)
    {
        uint32_t transfer_error = PC_swd_read_words(i, stride / 4, buf);
        pc_cb_offset = PC_find_cb_in_buf(buf, stride);

        if (pc_cb_offset != PC_CB_NOT_FOUND)
        { // header found!
            pc_cb_address = i + pc_cb_offset;
            uint32_t test;
            PC_swd_read_and_clear(pc_cb_address + 8, &test); // TODO: Test call, remove once done!
            break;
        }
    }
    free(buf);
    return pc_cb_address;
}