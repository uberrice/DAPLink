
#include "daplink.h"
#include "fsl_anactrl.h"
#include "fsl_power.h"
#include "fsl_lpadc.h"
#include "fsl_gpio.h"
#include "pin_mux.h"
#include "IO_Config.h"

#include "lpadc.h"
// #include "LPC55S69_cm33_core0_features.h"

uint16_t get_LPADC0B(void)
{
    lpadc_conv_result_t mLpadcResultConfigStruct;
    LPADC_DoSoftwareTrigger(ADC0, 1U); /* 1U is trigger0 mask. */
    while (!LPADC_GetConvResult(ADC0, &mLpadcResultConfigStruct, 0U))
    {
    }
    return (mLpadcResultConfigStruct.convValue >> 3U);
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

void LPADC0_Init(void)
{
    set_TargetPowerDisconnect(1);
    set_CalibrationResistorState(0);
    // initialize GPIO related to ADC
    
    // INITIALIZE ADC
    static lpadc_config_t mLpadcConfigStruct;
    static lpadc_conv_command_config_t mLpadcCommandConfigStruct;
    static lpadc_conv_trigger_config_t mLpadcTriggerConfigStruct;

    CLOCK_SetClkDiv(kCLOCK_DivAdcAsyncClk, 8U, true);
    CLOCK_AttachClk(kMAIN_CLK_to_ADC_CLK);

    POWER_DisablePD(kPDRUNCFG_PD_LDOGPADC);
    ANACTRL_Init(ANACTRL);
    //ANACTRL_EnableVref1V(ANACTRL, true); //disabled for now; needs some defines if it should be made to work
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
    mLpadcCommandConfigStruct.sampleTimeMode = kLPADC_SampleTimeADCK35;           // 131 clocks averaged - 1us per sample +-
    mLpadcCommandConfigStruct.hardwareAverageMode = kLPADC_HardwareAverageCount1; // 16 averaged, 16*sampletime value
    LPADC_SetConvCommandConfig(ADC0, 1, &mLpadcCommandConfigStruct);

    LPADC_GetDefaultConvTriggerConfig(&mLpadcTriggerConfigStruct);
    mLpadcTriggerConfigStruct.targetCommandId = 1;
    mLpadcTriggerConfigStruct.enableHardwareTrigger = false;
    LPADC_SetConvTriggerConfig(ADC0, 0U, &mLpadcTriggerConfigStruct); /* Configurate the trigger0. */

    set_TargetPowerDisconnect(1);
    set_LPADC0_currentMode(LPADC_current_high_sens);
    // // enable interrupts
    // LPADC_EnableInterrupts(ADC0, kLPADC_FIFO0WatermarkInterruptEnable);
    // EnableIRQ(ADC0_IRQn);
    // END INITIALIZE ADC
}

uint16_t LPADC_polling_current_read()
{
    lpadc_conv_result_t mLpadcResultConfigStruct;

    LPADC_DoSoftwareTrigger(ADC0, 1U); /* 1U is trigger0 mask. */
    while (!LPADC_GetConvResult(ADC0, &mLpadcResultConfigStruct, 0U));
    uint16_t retval = mLpadcResultConfigStruct.convValue;
    return retval;
}