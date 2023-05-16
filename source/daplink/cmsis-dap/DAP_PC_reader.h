#define PC_BUFFER_EMPTY (0)
#define PC_CB_NOT_FOUND (0)

uint32_t PC_find_control_block(uint32_t start_addr, uint32_t addr_range, uint32_t stride);
