#include "DAP.h"
#include <stddef.h>
#include <stdint.h>

#define RTT_MSG_BUF_SIZE (512) // minimum 128
#define RTT_HEADER_LENGTH (12)
#define RTT_MAX_NUM_UP_BUFFERS (8)
#define RTT_MAX_NUM_DOWN_BUFFERS (8)

#define RTT_CB_NOT_FOUND (0xFFFFFFFF)
#define RTT_FAILURE (0xDEADBEEF)
#define RTT_OK (0x00000000)

typedef struct
{
    uint32_t cb_address;        // Control block address
    uint32_t MaxNumUpBuffers;   // Maximum number of up-buffers (application-to-host)
    uint32_t MaxNumDownBuffers; // Maximum number of down-buffers (host-to-application)
                                // RTT buffer structures will follow
} SEGGER_RTT_CB;

typedef struct
{
    uint32_t address;      // Address where buffer descriptor is in target memory
    char *sName;           // Pointer to name of buffer
    char *pBuffer;         // Pointer to the buffer
    uint32_t SizeOfBuffer; // Size of the buffer
    uint32_t WrOff;        // Write position
    uint32_t RdOff;        // Read position
    int32_t Flags;         // Buffer flags
} SEGGER_RTT_BUFFER;

/*
 *  Get the a buffer from the DAP_queue where the response to the request is stored
 *    Parameters:      start_addr - Scan start address, addr_range - length of scan
 *    Return Value:    TODO
 */
uint32_t RTT_find_control_block(uint32_t start_addr, uint32_t addr_range);