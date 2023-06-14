#include <stdint.h>

#define SET_CALRES_1 (1<<0)
#define SET_CALRES_2 (1<<1)
#define SET_CALRES_3 (1<<2)
#define SET_CALRES_4 (1<<3)

uint16_t get_LPADC0B(void);

uint32_t get_LPADC0B_calibration_values(void);

/// @brief Sets the measurement range as a stopgap solution because ADC0_4 which can measure the jumper position is not implemented yet
/// @param range sets the measurement range, true = jumper position low
void set_measurement_range(bool range);

typedef enum _lpadc_current_mode
{
  LPADC_current_low_sens  = 0U,
  LPADC_current_high_sens = 1U,
} lpadc_current_mode_t;

/** Sets the state of calibration resistors
 *  R1 = 6.65k, R2 = 66.5, R3 = 1.65k, R4 = 16.5
 \param resistors the resistors to set; preferably use macros like SET_CALRES_1, 0x1 corresponds to resistor 1, 0x2 to resistor 2, 0x4 to resistor 3 and 0x8 to resistor 4
*/
void set_CalibrationResistorState(uint8_t resistors);


/// @brief Disconnects or reconnects the target's power
/// @param val target power connection, true = disconnected
void set_TargetPowerDisconnect(bool val);

void set_LPADC0_currentMode(lpadc_current_mode_t current_mode);

lpadc_current_mode_t get_LPADC0_currentMode(void);

/// @brief Initializes the LPADC0 peripheral in order to measure current
void LPADC0_Init(void);

/// @brief Initializes the pins used for LPADC0 in order to measure current, as well as GPIOs to set up the various options built into the MCU-Link Pro
void LPADC0_InitPins(void);


uint16_t LPADC_polling_current_read();