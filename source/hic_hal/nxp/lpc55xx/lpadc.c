
#include "daplink.h"
#include "fsl_anactrl.h"
#include "fsl_power.h"
#include "fsl_lpadc.h"
#include "fsl_gpio.h"
#include "pin_mux.h"
#include "IO_Config.h"
#include "DAP.h"

#include "lpadc.h"

static uint16_t calib_low_sens = 0;
static uint16_t calib_high_sens = 0;

static lpadc_current_mode_t current_mode_state = LPADC_current_high_sens;

//For jumper position:
static bool measurement_jumper = true; // true: low range, false = high range

uint16_t get_LPADC0B(void)
{
    return LPADC_polling_current_read();
}

uint32_t get_LPADC0B_calibration_values(void)
{
    uint32_t retval = (uint32_t) (calib_low_sens<<16);
    retval+=calib_high_sens;
    return retval;
}

void set_measurement_range(bool range) // true: low range, false = high range
{
    measurement_jumper = range;
}

void set_CalibrationResistorState(uint8_t resistors)
{
    GPIO_PinWrite(GPIO, CTRL_CAL_R1_PORT, CTRL_CAL_R1_PIN, (resistors >> 0) % 2);
    GPIO_PinWrite(GPIO, CTRL_CAL_R2_PORT, CTRL_CAL_R2_PIN, (resistors >> 1) % 2);
    GPIO_PinWrite(GPIO, CTRL_CAL_R3_PORT, CTRL_CAL_R3_PIN, (resistors >> 2) % 2);
    GPIO_PinWrite(GPIO, CTRL_CAL_R4_PORT, CTRL_CAL_R4_PIN, (resistors >> 3) % 2);
}


// true = disconnect, false = connect
void set_TargetPowerDisconnect(bool val){
    GPIO_PinWrite(GPIO, CTRL_CAL_DISC_PORT, CTRL_CAL_DISC_PIN, val); // ctrl_cal_disc
}


//High current mode = high sensitivity
void set_LPADC0_currentMode(lpadc_current_mode_t current_mode)
{
    current_mode_state = current_mode;
    if (current_mode == LPADC_current_high_sens)
    {
        // generate pulse on CTRL_HIGH_CURR_EN (FF clock) and disable MR
        GPIO_PinWrite(GPIO, CTRL_HIGH_CURR_DIS_PORT, CTRL_HIGH_CURR_DIS_PIN, 1); // disable MR
        GPIO_PinWrite(GPIO, CTRL_HIGH_CURR_EN_PORT, CTRL_HIGH_CURR_EN_PIN, 0);
        GPIO_PinWrite(GPIO, CTRL_HIGH_CURR_EN_PORT, CTRL_HIGH_CURR_EN_PIN, 1);
    }
    else // LPCADC_current_low
    {
        // turn HIGH_CURR_EN low then pulse HIGH_CURR_DIS
        GPIO_PinWrite(GPIO, CTRL_HIGH_CURR_EN_PORT, CTRL_HIGH_CURR_EN_PIN, 0);
        GPIO_PinWrite(GPIO, CTRL_HIGH_CURR_DIS_PORT, CTRL_HIGH_CURR_DIS_PIN, 0); // enable MR
        GPIO_PinWrite(GPIO, CTRL_HIGH_CURR_DIS_PORT, CTRL_HIGH_CURR_DIS_PIN, 1); // disable MR
    }
}

void LPADC0_calibrate(void)
{
    set_TargetPowerDisconnect(1);
    set_CalibrationResistorState(0);

    if(measurement_jumper) // TODO: implement check of Jumper position with ADC0_4
    {
        //'low' current range jumper
        set_LPADC0_currentMode(LPADC_current_high_sens);
        set_CalibrationResistorState(SET_CALRES_1);
        Delayms(10);
        calib_high_sens = get_LPADC0B();

        set_LPADC0_currentMode(LPADC_current_low_sens);
        set_CalibrationResistorState(SET_CALRES_2);
        Delayms(10);
        calib_low_sens = get_LPADC0B();
    }
    else
    {
        //'high' current range jumper
        set_LPADC0_currentMode(LPADC_current_high_sens);
        set_CalibrationResistorState(SET_CALRES_3);
        Delayms(10);
        calib_high_sens = get_LPADC0B();

        set_LPADC0_currentMode(LPADC_current_low_sens);
        set_CalibrationResistorState(SET_CALRES_4);
        Delayms(10);
        calib_low_sens = get_LPADC0B();
    }

    set_TargetPowerDisconnect(0);
}

void LPADC0_Init(void)
{
    // initialize GPIO related to ADC
    set_TargetPowerDisconnect(1);
    set_CalibrationResistorState(0);
    
    // INITIALIZE ADC
    static lpadc_config_t mLpadcConfigStruct;
    static lpadc_conv_command_config_t mLpadcCommandConfigStruct;
    static lpadc_conv_trigger_config_t mLpadcTriggerConfigStruct;

    CLOCK_SetClkDiv(kCLOCK_DivAdcAsyncClk, 8U, true);
    CLOCK_AttachClk(kMAIN_CLK_to_ADC_CLK);

    POWER_DisablePD(kPDRUNCFG_PD_LDOGPADC);
    ANACTRL_Init(ANACTRL);
    LPADC_GetDefaultConfig(&mLpadcConfigStruct);

    mLpadcConfigStruct.referenceVoltageSource = kLPADC_ReferenceVoltageAlt3;
    mLpadcConfigStruct.conversionAverageMode = kLPADC_ConversionAverage128;
    mLpadcConfigStruct.powerLevelMode = kLPADC_PowerLevelAlt4;
    mLpadcConfigStruct.enableAnalogPreliminary = true;
    LPADC_Init(ADC0, &mLpadcConfigStruct);
    LPADC_DoAutoCalibration(ADC0);
    LPADC_GetDefaultConvCommandConfig(&mLpadcCommandConfigStruct);
    mLpadcCommandConfigStruct.sampleChannelMode = kLPADC_SampleChannelSingleEndSideB;

    mLpadcCommandConfigStruct.conversionResolutionMode = kLPADC_ConversionResolutionHigh; // TODO: config change

    // setup variables for sample times
    mLpadcCommandConfigStruct.sampleTimeMode = kLPADC_SampleTimeADCK35; // 131 clocks averaged - 1us per sample +-
    mLpadcCommandConfigStruct.hardwareAverageMode = kLPADC_HardwareAverageCount1; // single reading
    LPADC_SetConvCommandConfig(ADC0, 1, &mLpadcCommandConfigStruct);

    LPADC_GetDefaultConvTriggerConfig(&mLpadcTriggerConfigStruct);
    mLpadcTriggerConfigStruct.targetCommandId = 1;
    mLpadcTriggerConfigStruct.enableHardwareTrigger = false;
    LPADC_SetConvTriggerConfig(ADC0, 0U, &mLpadcTriggerConfigStruct); /* Configurate the trigger0. */
    // END INITIALIZE ADC

    LPADC0_calibrate();
    set_TargetPowerDisconnect(0); //reconnects target power
    set_LPADC0_currentMode(LPADC_current_low_sens);
}

uint16_t LPADC_polling_current_read(void)
{
    lpadc_conv_result_t mLpadcResultConfigStruct;

    LPADC_DoSoftwareTrigger(ADC0, 1U); /* 1U is trigger0 mask. */
    while (!LPADC_GetConvResult(ADC0, &mLpadcResultConfigStruct, 0U));
    uint16_t retval = mLpadcResultConfigStruct.convValue;
    return retval;
}