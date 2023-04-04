
#include "DAP_config.h"
#include "DAP.h"
#include "debug_cm.h"


#define RTT_CONTROL_BLOCK_ADDR 0x20000000 // Replace with the actual address of the RTT control block in the target system


#if (DAP_SWD != 0)

//What do I want?
//DAP Transfer Block - Address and length as input
static uint32_t DAP_SWD_TransferBlock(uint32_t address, uint32_t blocks, uint8_t * response) {
  uint8_t* request;
  
  uint32_t  request_count;
  uint32_t  request_value;
  uint32_t  response_count;
  uint32_t  response_value;
  uint8_t  *response_head;
  uint32_t  retry;
  uint32_t  data;

  response_count = blocks;
  response_value = 0U;
  response_head  = response;
  response      += 3; 

  DAP_TransferAbort = 0U;

//   request++;            // Ignore DAP index

//   request_count = (uint32_t)(*(request+0) << 0) |
//                   (uint32_t)(*(request+1) << 8);
//   request += 2;
    if (request_count == 0U) {
        goto end; //if no transfers requested
    }

    request_value = *request++;
    // Read register block - reading data

    // Post AP read -- this is AP mode
    // Set up transfer
    retry = DAP_Data.transfer.retry_count;
    do {
    response_value = SWD_Transfer(request_value, NULL);
    } while ((response_value == DAP_TRANSFER_WAIT) && retry-- && !DAP_TransferAbort);
    if (response_value != DAP_TRANSFER_OK) {
    goto end;
    }

    while (request_count--) {
        // Read DP/AP register
        if ((request_count == 0U) && ((request_value & DAP_TRANSFER_APnDP) != 0U)) {
        // Last AP read
        request_value = DP_RDBUFF | DAP_TRANSFER_RnW;
        }
        retry = DAP_Data.transfer.retry_count;
        do {
        response_value = SWD_Transfer(request_value, &data);
        } while ((response_value == DAP_TRANSFER_WAIT) && retry-- && !DAP_TransferAbort);
        if (response_value != DAP_TRANSFER_OK) {
        goto end;
        }
        // Store data
        *response++ = (uint8_t) data;
        *response++ = (uint8_t)(data >>  8);
        *response++ = (uint8_t)(data >> 16);
        *response++ = (uint8_t)(data >> 24);
        response_count++;
    }

end:
  *(response_head+0) = (uint8_t)(response_count >> 0);
  *(response_head+1) = (uint8_t)(response_count >> 8);
  *(response_head+2) = (uint8_t) response_value;

  return ((uint32_t)(response - response_head));
}
#endif


// Process JTAG Transfer Block command and prepare response
//   request:  pointer to request data
//   response: pointer to response data
//   return:   number of bytes in response
#if (DAP_JTAG != 0)
// TODO: Implement JTAG transfer as well
#endif

uint32_t rtt_init(const uint8_t *request, uint8_t *response) {
    // RTT init
    // configure transfer?
    // locate control block
    for(uint32_t offset = 0; 
        offset < DAP_rtt_config.range; 
        offset += RTT_SCAN_STRIDE) {
    uint8_t request[] = [RTT_SCAN_BLOCK_SIZE];
    // prepare request data
    // Format: 
    // 1 byte: DAP Index (ignored for SWD)
    // 2 byte: count (32-bit values)
    // Info byte: DAP Access Port, Read Register; 0b11000000 - 0xC0 (maybe other way around, 0x03, test)
    // probably 0x03
    DAP_TransferBlock(rtt_scan_request, rtt_search_buf);
    //read block at addr+Offset into memory
    //check if chunk contains rtt_header
}

uint32_t read_rtt_data(uint32_t source_address, uint8_t *buffer, uint32_t length) {
    uint32_t bytes_read = 0;

    // Configure the access port for memory access
    if (DAP_Connect() && DAP_SWJ_Clock(1000000) && DAP_TransferConfigure(0, 16, 32) && DAP_SWJ_Sequence(50)) {
        // Configure the access port for memory access
        if (DAP_WriteDP(DP_SELECT, 0) && DAP_WriteAP(AP_CSW, 0x23000012) && DAP_WriteAP(AP_TAR, source_address)) {
            // Read RTT data from the target memory
            while (length > 0) {
                uint32_t data;
                if (DAP_ReadAP(AP_DRW, &data)) {
                    uint32_t chunk_size = (length > 4) ? 4 : length;

                    memcpy(buffer + bytes_read, &data, chunk_size);

                    bytes_read += chunk_size;
                    length -= chunk_size;
                    source_address += chunk_size;
                    DAP_WriteAP(AP_TAR, source_address);
                } else {
                    // Error occurred while reading the memory
                    break;
                }
            }
        }

        // Disconnect from the target
        DAP_Disconnect();
    }

    return bytes_read;
}



// Read up-buffers (application-to-host)
for (int32_t i = 0; i < rtt_cb.MaxNumUpBuffers; ++i) {
    SEGGER_RTT_BUFFER up_buffer;
    uint32_t buffer_address = rtt_cb_addr + sizeof(SEGGER_RTT_CB) + i * sizeof(SEGGER_RTT_BUFFER);
    bytes_read = read_rtt_data(buffer_address, (uint8_t *)&up_buffer, sizeof(up_buffer));

    if (bytes_read != sizeof(up_buffer)) {
        // Handle the error
    }

    uint32_t available_data = (up_buffer.WrOff >= up_buffer.RdOff) ? (up_buffer.WrOff - up_buffer.RdOff) : (up_buffer.SizeOfBuffer - up_buffer.RdOff);
    if (available_data > 0) {
        uint8_t data_buffer[available_data];

        // Read the RTT data
        bytes_read = read_rtt_data((uint32_t)up_buffer.pBuffer + up_buffer.RdOff, data_buffer, available_data
