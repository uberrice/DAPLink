
#include "DAP.h"
#include <stddef.h>
#include <stdint.h>

typedef struct {
    char        acID[16];     // Initialization string
    uint32_t     MaxNumUpBuffers;   // Maximum number of up-buffers (application-to-host)
    uint32_t     MaxNumDownBuffers; // Maximum number of down-buffers (host-to-application)
    // RTT buffer structures will follow
} SEGGER_RTT_CB;

typedef struct {
    char*       pBuffer;       // Pointer to the buffer
    uint32_t    SizeOfBuffer;  // Size of the buffer
    uint32_t    WrOff;         // Write position
    uint32_t    RdOff;         // Read position
    int32_t     Flags;         // Buffer flags
} SEGGER_RTT_BUFFER;