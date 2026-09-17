/******************************************************************************
 * Copyright (C) 2017 by LNLS - Brazilian Synchrotron Light Laboratory
 *
 * Redistribution, modification or use of this software in source or binary
 * forms is permitted as long as the files maintain this copyright. LNLS and
 * the Brazilian Center for Research in Energy and Materials (CNPEM) are not
 * liable for any misuse of this material.
 *
 *****************************************************************************/

/**
 * @file udc_c28.c
 * @brief Hardware abstraction level module for C28 core of DRS-UDC board
 * 
 * This module refers to the hardware abstraction level for C28 core of DRS-UDC
 * board. It includes all header files for C28 core provided by Texas
 * Instruments within controlSuite and defines an abstraction level for hardware
 * components from UDC board
 *
 * @author gabriel.brunheira
 * @date 24/10/2017
 *
 */

#include "udc_c28.h"
#include "pwm/pwm.h"
#include "control/control.h"

/**
 * Buzzer defines
 */
#define BUZZER_PITCH_FREQ       4000.0
#define BUZZER_MOD_FREQ         16.0

#define BUZZER_MOD_CLKDIV       0x7
#define BUZZER_MOD_HSPCLKDIV    0x5
#define BUZZER_MOD_PERIOD       7323


/**
 * Initialization of all digital inputs and outputs, including two debug GPIOs.
 */
void init_gpios(void)
{
    EALLOW;

    /// GPDO1 Digital Output 1 <=> UDC_GPIO67
    /// GPDO2 Digital Output 2 <=> UDC_GPIO65
    /// GPDO3 Digital Output 3 <=> UDC_GPIO66
    /// GPDO4 Digital Output 4 <=> UDC_GPIO64
    GpioCtrlRegs.GPCMUX1.all    = 0x00000000;
    GpioDataRegs.GPCCLEAR.all   = 0x0000000F;
    GpioCtrlRegs.GPCDIR.all     = 0x0000000F;

    /// GPDI1 Digital Input 1 <=> UDC_GPIO126
    GpioCtrlRegs.GPDMUX2.bit.GPIO126 = 0;
    GpioDataRegs.GPDCLEAR.bit.GPIO126 = 1;
    GpioCtrlRegs.GPDDIR.bit.GPIO126 = 0;

    /// GPDI2 Digital Input 2 <=> UDC_GPIO127
    GpioCtrlRegs.GPDMUX2.bit.GPIO127 = 0;
    GpioDataRegs.GPDCLEAR.bit.GPIO127 = 1;
    GpioCtrlRegs.GPDDIR.bit.GPIO127 = 0;

    /// GPDI3 Digital Input 3 <=> UDC_GPIO124
    GpioCtrlRegs.GPDMUX2.bit.GPIO124 = 0;
    GpioDataRegs.GPDCLEAR.bit.GPIO124 = 1;
    GpioCtrlRegs.GPDDIR.bit.GPIO124 = 0;

    /// GPDI4 Digital Input 4 <=> UDC_GPIO125
    GpioCtrlRegs.GPDMUX2.bit.GPIO125 = 0;
    GpioDataRegs.GPDCLEAR.bit.GPIO125 = 1;
    GpioCtrlRegs.GPDDIR.bit.GPIO125 = 0;

    /// GPDI5 Digital Input 5 <=> UDC_GPIO195
    GpioG2CtrlRegs.GPGMUX1.bit.GPIO195 = 0;
    GpioG2DataRegs.GPGCLEAR.bit.GPIO195 = 1;
    GpioG2CtrlRegs.GPGDIR.bit.GPIO195 = 0;

    /// GPDI6 Digital Input 6 <=> UDC_GPIO116
    GpioCtrlRegs.GPDMUX2.bit.GPIO116 = 0;
    GpioDataRegs.GPDCLEAR.bit.GPIO116 = 1;
    GpioCtrlRegs.GPDDIR.bit.GPIO116 = 0;

    /// GPDI7 Digital Input 7 <=> UDC_GPIO194
    GpioG2CtrlRegs.GPGMUX1.bit.GPIO194 = 0;
    GpioG2DataRegs.GPGCLEAR.bit.GPIO194 = 1;
    GpioG2CtrlRegs.GPGDIR.bit.GPIO194 = 0;

    /// GPDI8 Digital Input 8 <=> UDC_GPIO192
    GpioG2CtrlRegs.GPGMUX1.bit.GPIO192 = 0;
    GpioG2DataRegs.GPGCLEAR.bit.GPIO192 = 1;
    GpioG2CtrlRegs.GPGDIR.bit.GPIO192 = 0;

#if UDC_V2_1

    /// GPDI9 Digital Input 9 <=> UDC_GPIO109
    GpioCtrlRegs.GPDMUX1.bit.GPIO109 = 0;
    GpioDataRegs.GPDCLEAR.bit.GPIO109 = 1;
    GpioCtrlRegs.GPDDIR.bit.GPIO109 = 0;

    /// GPDI10 Digital Input 10 <=> UDC_GPIO110
    GpioCtrlRegs.GPDMUX1.bit.GPIO110 = 0;
    GpioDataRegs.GPDCLEAR.bit.GPIO110 = 1;
    GpioCtrlRegs.GPDDIR.bit.GPIO110 = 0;

#elif UDC_V2_0

    /// GPDI9 Digital Input 9 <=> UDC_GPIO115
    GpioCtrlRegs.GPDMUX2.bit.GPIO115 = 0;
    GpioDataRegs.GPDCLEAR.bit.GPIO115 = 1;
    GpioCtrlRegs.GPDDIR.bit.GPIO115 = 0;

    /// GPDI10 Digital Input 10 <=> UDC_GPIO114
    GpioCtrlRegs.GPDMUX2.bit.GPIO114 = 0;
    GpioDataRegs.GPDCLEAR.bit.GPIO114 = 1;
    GpioCtrlRegs.GPDDIR.bit.GPIO114 = 0;

#endif

    /// GPDI11 Digital Input 11 <=> UDC_GPIO113
    GpioCtrlRegs.GPDMUX2.bit.GPIO113 = 0;
    GpioDataRegs.GPDCLEAR.bit.GPIO113 = 1;
    GpioCtrlRegs.GPDDIR.bit.GPIO113 = 0;

    /// GPDI12 Digital Input 12 <=> UDC_GPIO112
    GpioCtrlRegs.GPDMUX2.bit.GPIO112 = 0;
    GpioDataRegs.GPDCLEAR.bit.GPIO112 = 1;
    GpioCtrlRegs.GPDDIR.bit.GPIO112 = 0;

    /// GPDI13 Digital Input 13 <=> UDC_GPIO197
    GpioG2CtrlRegs.GPGMUX1.bit.GPIO197 = 0;
    GpioG2DataRegs.GPGCLEAR.bit.GPIO197 = 1;
    GpioG2CtrlRegs.GPGDIR.bit.GPIO197 = 0;

    /// GPDI14 Digital Input 14 <=> UDC_GPIO196
    GpioG2CtrlRegs.GPGMUX1.bit.GPIO196 = 0;
    GpioG2DataRegs.GPGCLEAR.bit.GPIO196 = 1;
    GpioG2CtrlRegs.GPGDIR.bit.GPIO196 = 0;

    /// GPDI15 Digital Input 15 <=> UDC_GPIO198
    GpioG2CtrlRegs.GPGMUX1.bit.GPIO198 = 0;
    GpioG2DataRegs.GPGCLEAR.bit.GPIO198 = 1;
    GpioG2CtrlRegs.GPGDIR.bit.GPIO198 = 0;

    /// GPDI16 Digital Input 16 <=> UDC_GPIO199
    GpioG2CtrlRegs.GPGMUX1.bit.GPIO199 = 0;
    GpioG2DataRegs.GPGCLEAR.bit.GPIO199 = 1;
    GpioG2CtrlRegs.GPGDIR.bit.GPIO199 = 0;

    /// INT_ARM <=> UDC_GPIO28
    //GpioCtrlRegs.GPAMUX2.bit.GPIO28 = 0;
    //GpioDataRegs.GPACLEAR.bit.GPIO28 = 1;
    //GpioCtrlRegs.GPADIR.bit.GPIO28 = 0;

    /// INT_C28 <=> UDC_GPIO29
    //GpioCtrlRegs.GPAMUX2.bit.GPIO29 = 0;
    //GpioDataRegs.GPACLEAR.bit.GPIO29 = 1;
    //GpioCtrlRegs.GPADIR.bit.GPIO29 = 0;

    /// INT_GENERAL / SYNC_IN <=> UDC_GPIO55
    GpioCtrlRegs.GPBMUX2.bit.GPIO55 = 0;
    GpioDataRegs.GPBCLEAR.bit.GPIO55 = 1;
    GpioCtrlRegs.GPBDIR.bit.GPIO55 = 0;

    /// GPIO0 Debug GPIO 0 <=> UDC_GPIO46
    GpioCtrlRegs.GPBMUX1.bit.GPIO46 = 0;
    GpioDataRegs.GPBCLEAR.bit.GPIO46 = 1;
    GpioCtrlRegs.GPBDIR.bit.GPIO46 = 1;

    /// GPIO1 Debug GPIO 1 <=> UDC_GPIO111
    GpioCtrlRegs.GPDMUX1.bit.GPIO111 = 0;
    GpioDataRegs.GPDCLEAR.bit.GPIO111 = 1;
    GpioCtrlRegs.GPDDIR.bit.GPIO111 = 1;

    /// Initialize all PWM signals as digital outputs at low level, to prevent
    /// spurious signals. Then each PS module initialize its corresponding
    /// PWM signals according to the application.
    GpioDataRegs.GPACLEAR.all = 0x0000FFFF;
    GpioCtrlRegs.GPADIR.all  |= 0x0000FFFF;
    GpioCtrlRegs.GPAMUX1.all  = 0;
    EDIS;
}

/**
 * Initialization of buzzer from UDC v2_1 board. This function is ignored on
 * v2_0.
 *
 * @param volume: value between 0 and 100 [%] to indicate buzzer volume.
 */
void init_buzzer(float volume)
{
#if UDC_V2_1

    g_pwm_modules.num_modules = 8;

    disable_pwm_outputs();
    disable_pwm_tbclk();

    init_pwm_module(&EPwm9Regs, BUZZER_PITCH_FREQ, 0, PWM_Sync_Master, 0,
                  PWM_ChB_Independent, 0);
    init_pwm_module(&EPwm11Regs, 0.0, 0, PWM_Sync_Master, 0,
                  PWM_ChB_Independent, 0);

    EALLOW;

    /// Set frequency manually
    EPwm11Regs.TBPRD = BUZZER_MOD_PERIOD;

    /// Set clock pre-scaler, due to very low frequency
    EPwm11Regs.TBCTL.bit.CLKDIV = BUZZER_MOD_CLKDIV;
    EPwm11Regs.TBCTL.bit.HSPCLKDIV = BUZZER_MOD_HSPCLKDIV;

    /// Configure GPIOs
    GpioCtrlRegs.GPEMUX1.bit.GPIO128 = 1;
    GpioCtrlRegs.GPEMUX1.bit.GPIO132 = 1;

    /// Disable trip
    EPwm9Regs.TZSEL.bit.OSHT1 = 0;
    EPwm11Regs.TZSEL.bit.OSHT1 = 0;

    /// Enable PWM outputs
    EPwm9Regs.TZCLR.bit.OST = 1;
    EPwm11Regs.TZCLR.bit.OST = 1;

    SATURATE(volume,100.0,0.0);

    /// Set duty cycle
    set_pwm_duty_chA(&EPwm9Regs, volume/100.0);
    set_pwm_duty_chA(&EPwm11Regs, 0.5);

    EDIS;

#endif
}

/**
 * Initialization of EPWMSYNCI as general purpose digital input
 */
void cfg_epwmsynci_gpdi(void)
{
    EALLOW;

    #if UDC_V2_1
        GpioCtrlRegs.GPBMUX1.bit.GPIO38 = 0;
        GpioDataRegs.GPBCLEAR.bit.GPIO38 = 1;
        GpioCtrlRegs.GPBDIR.bit.GPIO38 = 0;
    #elif
        GpioCtrlRegs.GPBMUX1.bit.GPIO32 = 0;
        GpioDataRegs.GPBCLEAR.bit.GPIO32 = 1;
        GpioCtrlRegs.GPBDIR.bit.GPIO32 = 0;
    #endif

    EDIS;
}

/**
 * Initialization of EPWMSYNCO as general purpose digital output
 */
void cfg_epwmsynco_gpdo(void)
{
    EALLOW;

    GpioCtrlRegs.GPBMUX1.bit.GPIO33 = 0;
    GpioDataRegs.GPBCLEAR.bit.GPIO33 = 1;
    GpioCtrlRegs.GPBDIR.bit.GPIO33 = 1;

    EDIS;
}

/**
 * TODO: Put here the implementation for your private functions.
 */
