#include <stdint.h>

#define SET_CALRES_1 (1<<0)
#define SET_CALRES_2 (1<<1)
#define SET_CALRES_3 (1<<2)
#define SET_CALRES_4 (1<<3)

uint16_t get_LPADC0B(void);

typedef enum _lpadc_current_mode
{
  LPADC_current_low  = 0U,
  LPADC_current_high = 1U,
} lpadc_current_mode_t;


typedef struct _power_measurement_config
{
  lpadc_current_mode_t current_mode;
} power_measurement_config_t;



/** Sets the state of calibration resistors
 \param resistors the resistors to set; preferably use macros like SET_CALRES_1, 0x1 corresponds to resistor 1, 0x2 to resistor 2, 0x4 to resistor 3 and 0x8 to resistor 4
*/
void set_CalibrationResistorState(uint8_t resistors);

void LPADC0_Init(void);

uint16_t LPADC_polling_current_read();