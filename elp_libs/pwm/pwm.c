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
 * @file pwm.c
 * @brief PWM module
 * 
 * This module is responsible for configuration and operation of the 16 PWM
 * channels from DRS-UDC v2.1 board. It provides a higher level of abstraction
 * for the ePWM driver provided by controlSUITE.
 *
 * @author gabriel.brunheira
 * @date 24/11/2017
 *
 * TODO: Improve pwm modules access. Now it's using either EPWM_REGS pointers,
 * or pwm_module index (p.e. enable_pwm_output). Maybe use a dummy element like
 * SFOm in order to align indexes with EPWM_REGS numbering scheme.
 */

#include "pwm.h"

#define STATUS_SUCCESS  1
#define STATUS_FAIL     0
#define AUTOCONVERT     0   // 0: Off ; 1: On

#define TZ_ONE_SHOT     1

/**
 * The following three declarations are required in order to use the SFO library
 * functions.
 */

/**
 *  Scale factor resulted from HRPWM calibration.
 *
 *  Result can be used for all HRPWM channels. This variable is also copied to
 *  HRMSTEP register by SFO() function.
 */
int16_t MEP_ScaleFactor;

/**
 *  HRPWM calibration status
 */
uint16_t SFO_status;

/**
 * ePWM[0] is defined as dummy value not used in the example
 */
volatile struct EPWM_REGS *ePWM[9] = {  &EPwm1Regs, &EPwm1Regs, &EPwm2Regs,
                                        &EPwm3Regs, &EPwm4Regs, &EPwm5Regs,
                                        &EPwm6Regs, &EPwm7Regs, &EPwm8Regs};

/**
 * Struct to group all pwm modules information
 */
pwm_modules_t g_pwm_modules;

/**
 * Set frequency for specified PWM module. Also, returns period in system
 * clocks (125/150 MHz).
 *
 * @param p_pwm_module specified PWM module
 * @param freq switching frequency of pwm signal [Hz]
 * @return TBPRD register value for specified frequency
 */
uint16_t set_pwm_freq(volatile struct EPWM_REGS *p_pwm_module, double freq)
{
    uint16_t period;
    period = ((double) C28_FREQ_MHZ * (double) 1E6) / (freq * (double) (0x1 << p_pwm_module->TBCTL.bit.CLKDIV)) - 1;
    p_pwm_module->TBPRD = period;
    return period;
}

/**
 * Set dead time between channel A and B from specified PWM module. It applies
 * only when channel B configured as complementary. See `cfg_pwm_channel_b()`
 * function)
 *
 * @param p_pwm_module specified PWM module
 * @param deadtime dead-time between channel A and B [ns]
 */
void set_pwm_deadtime(volatile struct EPWM_REGS *p_pwm_module, uint16_t deadtime)
{
    uint16_t dt_clk;
    dt_clk = ((float) ((Uint32) C28_FREQ_MHZ * deadtime) * 1E-3) / ((double) (0x1 << p_pwm_module->TBCTL.bit.CLKDIV));
    p_pwm_module->DBFED = dt_clk;         // Falling-edge
    p_pwm_module->DBRED = dt_clk;         // Rising-edge
}

/**
 * Set dead time between channel A and B from specified PWM module. It applies
 * only when channel B configured as complementary. See `cfg_pwm_channel_b()`
 * function)
 *
 * @param p_pwm_module specified PWM module
 * @param deadtime dead-time between channel A and B [ns]
 */
void set_pwm_deadtime_edge(volatile struct EPWM_REGS *p_pwm_module, uint16_t deadtime_rising, uint16_t deadtime_falling)
{
	uint16_t dt_clk_rising;
	uint16_t dt_clk_falling;
	dt_clk_rising = ((float) ((Uint32) C28_FREQ_MHZ * deadtime_rising) * 1E-3) / ((double) (0x1 << p_pwm_module->TBCTL.bit.CLKDIV));
	dt_clk_falling = ((float) ((Uint32) C28_FREQ_MHZ * (deadtime_falling - deadtime_rising)) * 1E-3) / ((double) (0x1 << p_pwm_module->TBCTL.bit.CLKDIV));
	p_pwm_module->DBRED = dt_clk_rising;         // Rising-edge
	p_pwm_module->DBFED = dt_clk_falling;        // Falling-edge
}

/**
 * Configure counter synchronization of specified PWM module. See section
 * 7.1.4.3.2 (Time-Base Counter Synchronization) from F28M36x Technical
 * Reference for more information.
 *
 * @param p_pwm_module specified PWM module
 * @param sync_mode synchronization mode [`PWM_Sync_Master`, `PWM_Sync_Slave`]
 * @param phase_degrees phase between modules[º]
 */
void cfg_pwm_sync(volatile struct EPWM_REGS *p_pwm_module, pwm_sync_t sync_mode,
                  uint16_t phase_degrees)
{
    uint16_t phase;

    switch(sync_mode)
    {
        case PWM_Sync_Master:
        {
            p_pwm_module->TBPHS.half.TBPHS = 0;
            p_pwm_module->TBCTL.bit.PHSEN = TB_DISABLE;
            p_pwm_module->TBCTL.bit.SYNCOSEL = TB_CTR_ZERO;
            break;
        }

        case PWM_Sync_Slave:
        {
            //phase = (float) phase_degrees * ((float) p_pwm_module->TBPRD / 360.0);
            phase = (360.0 - (float) phase_degrees) *
                    ((float) p_pwm_module->TBPRD /360.0);
            p_pwm_module->TBPHS.half.TBPHS = phase;
            p_pwm_module->TBCTL.bit.PHSEN = TB_ENABLE;
            p_pwm_module->TBCTL.bit.SYNCOSEL = TB_SYNC_IN;
            break;
        }
    }
}

/**
 * Configure channel B of specified PWM module either as independent, or
 * complementary from channel A. See `set_pwm_deadtime()` function.
 *
 * @param p_pwm_module specified PWM module
 * @param cfg_channel_b channel B configuration [`PWM_ChB_Independent`,
 * `PWM_ChB_Complementary`]
 */
void cfg_pwm_channel_b(volatile struct EPWM_REGS *p_pwm_module,
                       cfg_pwm_channel_b_t cfg_channel_b)
{
    switch(cfg_channel_b)
    {
        case PWM_ChB_Independent:
        {
            p_pwm_module->DBCTL.bit.IN_MODE   = DBA_RED_DBB_FED;
            p_pwm_module->DBCTL.bit.POLSEL    = DB_ACTV_HI;
            p_pwm_module->DBCTL.bit.OUT_MODE  = DB_DISABLE;
            p_pwm_module->DBCTL.bit.OUTSWAP   = 0;
            break;
        }

        case PWM_ChB_Complementary:
        {
            p_pwm_module->DBCTL.bit.IN_MODE   = DBA_ALL;
            p_pwm_module->DBCTL.bit.POLSEL    = DB_ACTV_HIC;
            p_pwm_module->DBCTL.bit.OUT_MODE  = DB_FULL_ENABLE;
            p_pwm_module->DBCTL.bit.OUTSWAP   = 0;
            break;
        }

        case PWM_ChB_Complementary_Swapped:
        {
            p_pwm_module->DBCTL.bit.IN_MODE   = DBA_ALL;
            p_pwm_module->DBCTL.bit.POLSEL    = DB_ACTV_HIC;
            p_pwm_module->DBCTL.bit.OUT_MODE  = DB_FULL_ENABLE;
            p_pwm_module->DBCTL.bit.OUTSWAP   = 3;
            break;
        }

    }
}

/**
 * Link given PWM module with specified primary module. Each time the period or
 * compare registers are updated in the primary module, the linked registers
 * automatically get updated with the same value. To keep it independent, use
 * primary_module = 0.
 *
 * @param p_pwm_module specified PWM module
 * @param primary_module specified primary module. For independent
 * configuration, use 0.
 */
void set_pwm_primary_module(volatile struct EPWM_REGS *p_pwm_module,
                            uint16_t primary_module)
{
    uint16_t aux = primary_module + 1;

    p_pwm_module->EPWMXLINK.bit.TBPRDLINK = aux;
    p_pwm_module->EPWMXLINK.bit.CMPALINK  = aux;
    p_pwm_module->EPWMXLINK.bit.CMPBLINK  = aux;
}

/**
 * Set duty-cycle (0.0 to 1.0) for channel A of specified PWM module.
 *
 * @param p_pwm_module specified PWM module
 * @param duty specified duty cycle [p.u.]
 */
void set_pwm_duty_chA(volatile struct EPWM_REGS *p_pwm_module, float duty_pu)
{
    uint16_t duty_int;
    uint16_t duty_frac;
    float duty;

    duty = duty_pu * (float)p_pwm_module->TBPRD;

    duty_int  = (uint16_t) duty;
    duty_frac = ((uint16_t) ((duty - (float)duty_int) * MEP_ScaleFactor)) << 8;
    duty_frac += 0x0180;

    p_pwm_module->CMPAM2.half.CMPA    = duty_int;
    p_pwm_module->CMPAM2.half.CMPAHR  = duty_frac;
}

/**
 * Set duty-cycle (0.0 to 1.0) for channel B of specified PWM module.
 *
 * @param p_pwm_module specified PWM module
 * @param duty specified duty cycle [p.u.]
 */
void set_pwm_duty_chB(volatile struct EPWM_REGS *p_pwm_module, float duty_pu)
{
    uint16_t duty_int;
    uint16_t duty_frac;
    float duty;

    duty = duty_pu * (float)p_pwm_module->TBPRD;

    duty_int  = (uint16_t) duty;
    duty_frac = ((uint16_t) ((duty - (float)duty_int) * MEP_ScaleFactor)) << 8;
    duty_frac += 0x0180;

    p_pwm_module->CMPBM.half.CMPB     = duty_int;
    p_pwm_module->CMPBM.half.CMPBHR   = duty_frac;
}

/**
 * Set duty-cycle (-1.0 to 1.0) for specified PWM module working with a H-bridge
 * with unipolar switching scheme. Channel B must be set as complementary using
 * `cfg_pwm_channel_b()` function.
 *
 * @param p_pwm_module specified PWM module
 * @param duty specified duty cycle [p.u.]
 */
void set_pwm_duty_hbridge(volatile struct EPWM_REGS *p_pwm_module, float duty_pu)
{
    uint16_t duty_int;
    uint16_t duty_frac;
    float duty;

    duty = (0.5 * duty_pu + 0.5) * (float)p_pwm_module->TBPRD;

    duty_int  = (uint16_t) duty;
    duty_frac = ((uint16_t) ((duty - (float)duty_int) * MEP_ScaleFactor)) << 8;
    duty_frac += 0x0180;

    p_pwm_module->CMPAM2.half.CMPA    = duty_int;
    p_pwm_module->CMPAM2.half.CMPAHR  = duty_frac;
}

/*
 *
 *
 *
 */

/**
 * Initialization of specified PWM module. This function initializes both
 * channels with digital counter configured as a saw-tooth carrier waveform.
 *
 * @param p_pwm_module specified PWM module
 * @param freq switching frequency of pwm signal [Hz]
 * @param primary_module specified primary module. For independent
 * @param sync_mode synchronization mode [`PWM_Sync_Master`/`PWM_Sync_Slave`]
 * @param phase_degrees phase between modules[º]
 * @param cfg_channel_b channel B configuration [`PWM_ChB_Independent`,
 * `PWM_ChB_Complementary`]
 * @param deadtime dead-time between channel A and B [ns]
 */
void init_pwm_module(volatile struct EPWM_REGS *p_pwm_module, double freq,
                     uint16_t primary_module, pwm_sync_t sync_mode,
                     uint16_t phase_degrees, cfg_pwm_channel_b_t cfg_channel_b,
                     uint16_t deadtime)
{
	uint16_t TB_DIV_N;

	if(freq < 2288 && freq >= 1144)
	{
		TB_DIV_N = TB_DIV2;
	}
	else if(freq < 1144 && freq >= 572)
	{
		TB_DIV_N = TB_DIV4;
	}
	else if(freq < 572 && freq >= 286)
	{
		TB_DIV_N = TB_DIV8;
	}
	else
	{
		TB_DIV_N = TB_DIV1;
	}

	/* Counter-register configuration */
    p_pwm_module->TBCTL.bit.HSPCLKDIV = TB_DIV1;
    p_pwm_module->TBCTL.bit.CLKDIV = TB_DIV_N;
    p_pwm_module->TBCTL.bit.PRDLD = TB_IMMEDIATE;
    p_pwm_module->TBCTL.bit.CTRMODE = TB_COUNT_UP;
    p_pwm_module->TBCTR = 0;

    /* Parametric configuration */
    set_pwm_freq(p_pwm_module,freq);
    set_pwm_duty_chA(p_pwm_module,0.0);
    set_pwm_duty_chB(p_pwm_module,0.0);
    cfg_pwm_sync(p_pwm_module, sync_mode, phase_degrees);
    cfg_pwm_channel_b(p_pwm_module, cfg_channel_b);
    set_pwm_deadtime(p_pwm_module, deadtime);
    set_pwm_primary_module(p_pwm_module, primary_module);

    /* Action-Qualifier configuration */
    p_pwm_module->AQCTLA.bit.ZRO = AQ_SET;
    p_pwm_module->AQCTLA.bit.PRD = AQ_NO_ACTION;
    p_pwm_module->AQCTLA.bit.CAU = AQ_CLEAR;
    p_pwm_module->AQCTLA.bit.CAD = AQ_NO_ACTION;
    p_pwm_module->AQCTLA.bit.CBU = AQ_NO_ACTION;
    p_pwm_module->AQCTLA.bit.CBD = AQ_NO_ACTION;

    p_pwm_module->AQCTLB.bit.ZRO = AQ_SET;
    p_pwm_module->AQCTLB.bit.PRD = AQ_NO_ACTION;
    p_pwm_module->AQCTLB.bit.CAU = AQ_NO_ACTION;
    p_pwm_module->AQCTLB.bit.CAD = AQ_NO_ACTION;
    p_pwm_module->AQCTLB.bit.CBU = AQ_CLEAR;
    p_pwm_module->AQCTLB.bit.CBD = AQ_NO_ACTION;

    /* Compare registers configuration */
    p_pwm_module->CMPCTL.bit.SHDWAMODE = CC_SHADOW;
    p_pwm_module->CMPCTL.bit.SHDWBMODE = CC_SHADOW;
    p_pwm_module->CMPCTL.bit.LOADAMODE = CC_CTR_ZERO;
    p_pwm_module->CMPCTL.bit.LOADBMODE = CC_CTR_ZERO;

    /* Interruption configuration */
    p_pwm_module->ETSEL.bit.INTSEL = ET_CTR_ZERO;       // Select INT on Zero event
    p_pwm_module->ETSEL.bit.INTEN = 0;                  // Disable INT
    p_pwm_module->ETPS.bit.INTPRD = ET_1ST;             // Generate INT on 1st event

    EALLOW;

    /* High-resolution feature configuration */
    p_pwm_module->HRCNFG.all = 0x0;
    p_pwm_module->HRCNFG.bit.EDGMODE = HR_FEP;                // MEP control on both edges
    p_pwm_module->HRCNFG.bit.EDGMODEB = HR_REP;               // MEP control on both edges
    p_pwm_module->HRCNFG.bit.HRLOAD  = HR_CTR_ZERO;           // load on CTR = 0
    p_pwm_module->HRCNFG.bit.HRLOADB  = HR_CTR_ZERO;          // load on CTR = 0
    p_pwm_module->HRCNFG.bit.AUTOCONV = 0;                    // Enable autoconversion for HR period
    p_pwm_module->HRCNFG.bit.CTLMODE = HR_CMP;                // CMPAHR and TBPRDHR HR control
    p_pwm_module->HRCNFG.bit.CTLMODEB = HR_CMP;               // CMPBHR and TBPRDHR HR control
    p_pwm_module->HRCNFG.bit.HRLOAD  = HR_CTR_ZERO_PRD;       // load on CTR = 0 and CTR = TBPRD
    p_pwm_module->HRCNFG.bit.HRLOADB  = HR_CTR_ZERO_PRD;      // load on CTR = 0 and CTR = TBPRD

    /* Trip Zone - used to enable/disable PWM outputs via software */
    //GpioG1TripRegs.GPTRIP1SEL.bit.GPTRIP1SEL = 29;            // GPIO29
    p_pwm_module->TZSEL.bit.OSHT1 = 0;                        // One-shot trip
    p_pwm_module->TZCTL.bit.TZA = TZ_FORCE_LO;                //
    p_pwm_module->TZCTL.bit.TZB = TZ_FORCE_LO;                // PWM outputs forced to low state during trip.
    p_pwm_module->TZFRC.bit.OST = 1;
    EDIS;
}

/**
 * Enable outputs from specified PWM module.
 *
 * @param pwm_module specified PWM module
 */
void enable_pwm_output(uint16_t pwm_module)
{
    // Clear trip flag, enabling PWM outputs
    EALLOW;
    g_pwm_modules.pwm_regs[pwm_module]->TZCLR.bit.OST = 1;
    g_pwm_modules.pwm_state[pwm_module] = PWM_ENABLED;
    EDIS;
}

/**
 * Disable outputs from specified PWM module.
 *
 * @param pwm_module specified PWM module
 */
void disable_pwm_output(uint16_t pwm_module)
{
    // Force trip via software, disabling PWM outputs
    EALLOW;
    g_pwm_modules.pwm_regs[pwm_module]->TZFRC.bit.OST = 1;
    g_pwm_modules.pwm_state[pwm_module] = PWM_DISABLED;
    EDIS;
}


/**
 * Enable outputs from all PWM modules
 */
void enable_pwm_outputs(void)
{
    uint16_t i;

    // Clear trip flags, enabling PWM outputs
    EALLOW;
    for(i = 0; i < g_pwm_modules.num_modules; i++)
    {
        g_pwm_modules.pwm_regs[i]->TZCLR.bit.OST = 1;
        g_pwm_modules.pwm_state[i] = PWM_ENABLED;
    }
    EDIS;
}

/**
 * Disable outputs from all PWM modules
 */
void disable_pwm_outputs(void)
{
    uint16_t i;

    // Force trip via software, disabling PWM outputs
    EALLOW;
    for(i = 0; i < g_pwm_modules.num_modules; i++)
    {
        g_pwm_modules.pwm_regs[i]->TZFRC.bit.OST = 1;
        g_pwm_modules.pwm_state[i] = PWM_DISABLED;
    }
    EDIS;
}


/**
 * Enable time-base clock for PWM modules
 */
void enable_pwm_tbclk(void)
{
    uint16_t i;

    for(i = 0; i < NUM_MAX_PWM_MODULES; i++)
    {
        g_pwm_modules.pwm_regs[i]->TBCTR = g_pwm_modules.pwm_regs[i]->TBPHS.half.TBPHS;
        g_pwm_modules.pwm_regs[i]->ETCLR.bit.INT = 1;
    }

    PieCtrlRegs.PIEIFR3.all = 0x0000;
    PieCtrlRegs.PIEACK.all |= M_INT3;
    IFR &= ~M_INT3;

    EALLOW;
    SysCtrlRegs.PCLKCR0.bit.TBCLKSYNC = 1;
    EDIS;
}

/**
 * Disable time-base clock for PWM modules
 */
void disable_pwm_tbclk(void)
{
    uint16_t i;

    EALLOW;
    SysCtrlRegs.PCLKCR0.bit.TBCLKSYNC = 0;
    EDIS;

    /* Adicionado 11/03/2016, vindo do FW da FAC/DCDC */
    for(i = 0; i < NUM_MAX_PWM_MODULES; i++)
    {
        g_pwm_modules.pwm_regs[i]->TBCTR = g_pwm_modules.pwm_regs[i]->TBPHS.half.TBPHS;
        g_pwm_modules.pwm_regs[i]->ETCLR.bit.INT = 1;
    }

    PieCtrlRegs.PIEIFR3.all = 0x0000;
    PieCtrlRegs.PIEACK.all |= M_INT3;
    IFR &= ~M_INT3;
}

/**
 * Enable interrupts for the specified PWM module.
 *
 * @param p_pwm_module specified PWM module
 */
void enable_pwm_interrupt(volatile struct EPWM_REGS *p_pwm_module)
{
    EALLOW;
    p_pwm_module->ETSEL.bit.INTEN = 1;
    EDIS;
}

/**
 * Disable interrupts for the specified PWM module.
 *
 * @param p_pwm_module specified PWM module
 */
void disable_pwm_interrupt(volatile struct EPWM_REGS *p_pwm_module)
{
    EALLOW;
    p_pwm_module->ETSEL.bit.INTEN = 0;
    p_pwm_module->ETCLR.bit.INT = 1;
    EDIS;
}

/**
 * Initialization of calibration algorithm (SFO) for HRPWM MEP
 *
 * TODO: check error
 */
void init_pwm_mep_sfo(void)
{
    SFO_status = SFO_INCOMPLETE;
    while(SFO_status == SFO_INCOMPLETE)
    {
        SFO_status = SFO();
        if (SFO_status == SFO_ERROR)
        {
            //error();      // SFO function returns 2 if an error occurs & # of MEP steps/coarse step
        }                   // exceeds maximum of 255.
    }
}


/**
 * Runs calibration algorithm (SFO) for HRPWM MEP
 *
 * TODO: check
 */
void tune_pwm_mep_sfo(void)
{
    /*SFO_status = SFO();
    if (SFO_status == SFO_ERROR)
    {
        //error();
    }
    return;*/
}
