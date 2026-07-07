/******************************************************************************
 * Copyright (C) 2026 by LNLS - Brazilian Synchrotron Light Laboratory
 *
 * Redistribution, modification or use of this software in source or binary
 * forms is permitted as long as the files maintain this copyright. LNLS and
 * the Brazilian Center for Research in Energy and Materials (CNPEM) are not
 * liable for any misuse of this material.
 *
 *****************************************************************************/

/**
 * @file resonant_swls.h
 * @brief Resonant converter module for Superconductinng Wind Generator (SWG)
 *
 * Module for control of resonant convert power supply designed for the
 * Superconductinng Wind Generator (SWG). It implements the controller for load
 * current using frequency modulation strategy (FMS), instead of pulse-width
 * modulation (PWM).
 *
 * @author lucas.carnevalli
 * @date 26/05/2026
 *
 */

#include <float.h>

#include "boards/udc_c28.h"
#include "common/structs.h"
#include "common/timeslicer.h"
#include "control/control.h"
#include "event_manager/event_manager.h"
#include "HRADC_board/HRADC_Boards.h"
#include "ipc/ipc.h"
#include "parameters/parameters.h"
#include "pwm/pwm.h"

#include "resonant_swls.h"

/**
 * Analog variables parameters
 */
#define MAX_ILOAD                               ANALOG_VARS_MAX[0]

#define MAX_V_DCLINK                            ANALOG_VARS_MAX[1]
#define MIN_V_DCLINK                            ANALOG_VARS_MIN[1]

#define MAX_V_DCLINK_TURN_ON                    ANALOG_VARS_MAX[2]

#define NUM_DCCTs                               ANALOG_VARS_MAX[3]  // > 0  : 2 DCCT's

#define MAX_DCCTS_DIFF                          ANALOG_VARS_MAX[4]

#define MAX_I_IDLE_DCCT                         ANALOG_VARS_MAX[5]
#define MIN_I_ACTIVE_DCCT                       ANALOG_VARS_MIN[5]

/// Fixed turn-on time for discontinuous-conduction mode operation [us]
#define T_ON_US                                 ANALOG_VARS_MAX[6]

/// Fixed frequency offset summed to control effort to compensate dead-zone [Hz]
#define FREQ_DEADZONE_HZ                        ANALOG_VARS_MAX[7]

#define TIMEOUT_CONTACTOR_K1_CLOSED_MS          ANALOG_VARS_MAX[8]
#define TIMEOUT_CONTACTOR_K1_OPENED_MS          ANALOG_VARS_MAX[9]
#define RESET_PULSE_TIME_CONTACTOR_K1_MS        ANALOG_VARS_MAX[10]

/// Dead time for PWMs -> rising and falling edge [ns]
#define PWM_DEAD_TIME_RISING                    ANALOG_VARS_MIN[2]
#define PWM_DEAD_TIME_FALLING                   ANALOG_VARS_MIN[3]

/// Dead time for Active Rectifier PWMs -> rising and falling edge [ns]
#define PWM_DEAD_TIME_RISING_ACT_RCT            ANALOG_VARS_MIN[6]
#define PWM_DEAD_TIME_FALLING_ACT_RCT           ANALOG_VARS_MIN[7]

/// used for compensate dead-zone [Hz]
#define MAX_REF_CL                              ANALOG_VARS_MAX[11]
#define MIN_REF_CL                              ANALOG_VARS_MIN[4]

/**
 * Controller defines
 */

/// DSP Net Signals
#define I_LOAD_1                g_controller_ctom.net_signals[0].f  // HRADC0 - HALL SENSOR
#define I_LOAD_2                g_controller_ctom.net_signals[1].f  // HRADC1 - HALL SENSOR
#define V_DCLINK                g_controller_ctom.net_signals[2].f  // HRADC2 - VERIVOLT

#define I_LOAD_MEAN             g_controller_ctom.net_signals[3].f
#define I_LOAD_ERROR            g_controller_ctom.net_signals[4].f
#define I_LOAD_DIFF             g_controller_ctom.net_signals[5].f

#define FREQ_MODULATED          g_controller_ctom.net_signals[6].f

#define FREQ_MODULATED_COMPENS  g_controller_ctom.net_signals[7].f

#define FREQ_MODULATED_FF       g_controller_ctom.output_signals[0].f

/// Reference
#define I_LOAD_SETPOINT             g_ipc_ctom.ps_module[0].ps_setpoint
#define I_LOAD_REFERENCE            g_ipc_ctom.ps_module[0].ps_reference

#define SRLIM_I_LOAD_REFERENCE      &g_controller_ctom.dsp_modules.dsp_srlim[0]

#define WFMREF                      g_ipc_ctom.wfmref[0]

#define SIGGEN                      SIGGEN_CTOM[0]
#define SRLIM_SIGGEN_AMP            &g_controller_ctom.dsp_modules.dsp_srlim[1]
#define SRLIM_SIGGEN_OFFSET         &g_controller_ctom.dsp_modules.dsp_srlim[2]

#define MAX_SLEWRATE_SLOWREF            g_controller_mtoc.dsp_modules.dsp_srlim[0].coeffs.s.max_slewrate
#define MAX_SLEWRATE_SIGGEN_AMP         g_controller_mtoc.dsp_modules.dsp_srlim[1].coeffs.s.max_slewrate
#define MAX_SLEWRATE_SIGGEN_OFFSET      g_controller_mtoc.dsp_modules.dsp_srlim[2].coeffs.s.max_slewrate

/// Load current controller
#define ERROR_I_LOAD                    &g_controller_ctom.dsp_modules.dsp_error[0]

#define PI_CONTROLLER_I_LOAD            &g_controller_ctom.dsp_modules.dsp_pi[0]
#define PI_CONTROLLER_I_LOAD_COEFFS     g_controller_mtoc.dsp_modules.dsp_pi[0].coeffs.s
#define KP_I_LOAD                       PI_CONTROLLER_I_LOAD_COEFFS.kp
#define KI_I_LOAD                       PI_CONTROLLER_I_LOAD_COEFFS.ki

/// DC-link voltage feedforward controller
#define FF_V_DCLINK                     &g_controller_ctom.dsp_modules.dsp_ff[0]
#define FF_V_DCLINK_COEFFS              g_controller_mtoc.dsp_modules.dsp_ff[0].coeffs.s
#define NOM_V_DCLINK_FF                 FF_V_DCLINK_COEFFS.vdc_nom
#define MIN_V_DCLINK_FF                 FF_V_DCLINK_COEFFS.vdc_min

/// PWM modulators
#define PWM_MODULATOR_1                 g_pwm_modules.pwm_regs[0]
#define PWM_MODULATOR_2                 g_pwm_modules.pwm_regs[1]
#define PWM_MODULATOR_3                 g_pwm_modules.pwm_regs[2]
#define PWM_MODULATOR_4                 g_pwm_modules.pwm_regs[3]
#define PWM_MODULATOR_5                 g_pwm_modules.pwm_regs[4]
#define PWM_MODULATOR_6                 g_pwm_modules.pwm_regs[5]
#define PWM_ISR_CONTROLLER              g_pwm_modules.pwm_regs[6]

/// Scope
#define SCOPE                           SCOPE_CTOM[0]

/**
 * Digital I/O's status
 */
#define PIN_OPEN_CONTACTOR_K1           CLEAR_GPDO1
#define PIN_CLOSE_CONTACTOR_K1          SET_GPDO1

#define PIN_STATUS_CONTACTOR_K1         GET_GPDI5

#define PIN_STATUS_CONTACTOR_K2         GET_GPDI6

#define PIN_STATUS_EXTERNAL_INTERLOCK   GET_GPDI7

#define PIN_STATUS_DCCT_1_STATUS        GET_GPDI9
#define PIN_STATUS_DCCT_1_ACTIVE        GET_GPDI10
#define PIN_STATUS_DCCT_2_STATUS        GET_GPDI11
#define PIN_STATUS_DCCT_2_ACTIVE        GET_GPDI12

/**
 * Interlocks and alarms defines
 */
typedef enum
{
    Load_Overcurrent,
    DCLink_Overvoltage,
    DCLink_Undervoltage,
    Welded_Contactor_K1_Fault,
	Welded_Contactor_K2_Fault,
    Opened_Contactor_K1_Fault,
	Opened_Contactor_K2_Fault,
    External_Itlk,
    IIB_Itlk
} hard_interlocks_t;

typedef enum
{
    DCCT_1_Fault,
    DCCT_2_Fault,
    DCCT_High_Difference,
    Load_Feedback_1_Fault,
    Load_Feedback_2_Fault
} soft_interlocks_t;

typedef enum
{
    High_Sync_Input_Frequency = 0x00000001
} alarms_t;

#define NUM_HARD_INTERLOCKS             IIB_Itlk + 1
#define NUM_SOFT_INTERLOCKS             Load_Feedback_2_Fault + 1

/**
 *  Private variables
 */
static uint16_t decimation_factor;
static float decimation_coeff;

/**
 * Private functions
 */
#pragma CODE_SECTION(isr_init_controller, "ramfuncs");
#pragma CODE_SECTION(isr_controller, "ramfuncs");
#pragma CODE_SECTION(turn_off, "ramfuncs");

static void init_peripherals_drivers(void);
static void term_peripherals_drivers(void);

static void init_controller(void);
static void reset_controller(void);
static void enable_controller();
static void disable_controller();
static interrupt void isr_init_controller(void);
static interrupt void isr_controller(void);

static void init_interruptions(void);
static void term_interruptions(void);

static void turn_on(uint16_t dummy);
static void turn_off(uint16_t dummy);

static void reset_interlocks(uint16_t dummy);
static inline void check_interlocks(void);

/**
 * Main function for this power supply module
 */
void main_resonant_swls(void)
{
    init_controller();
    init_peripherals_drivers();
    init_interruptions();
    reset_controller();
    enable_controller();

    /// TODO: check why first sync_pulse occurs
    g_ipc_ctom.counter_sync_pulse = 0;

    /// TODO: include condition for re-initialization
    while(1)
    {
        check_interlocks();
    }

    turn_off(0);

    disable_controller();
    term_interruptions();
    reset_controller();
    term_peripherals_drivers();
}

static void init_peripherals_drivers(void)
{
    uint16_t i;
    float duty_cycle;

    /// Initialization of HRADC boards
    stop_DMA();

    decimation_factor = (uint16_t) roundf(HRADC_FREQ_SAMP / ISR_CONTROL_FREQ);
    decimation_coeff = 1.0 / (float) decimation_factor;

    HRADCs_Info.enable_Sampling = 0;
    HRADCs_Info.n_HRADC_boards = NUM_HRADC_BOARDS;

    Init_DMA_McBSP_nBuffers(NUM_HRADC_BOARDS, decimation_factor, HRADC_SPI_CLK);

    Init_SPIMaster_McBSP(HRADC_SPI_CLK);
    Init_SPIMaster_Gpio();
    InitMcbspa20bit();

    DELAY_US(500000);
    send_ipc_lowpriority_msg(0,Enable_HRADC_Boards);
    DELAY_US(2000000);

    for(i = 0; i < NUM_HRADC_BOARDS; i++)
    {
        Init_HRADC_Info(&HRADCs_Info.HRADC_boards[i], i, decimation_factor,
                        buffers_HRADC[i], TRANSDUCER_GAIN[i]);
        Config_HRADC_board(&HRADCs_Info.HRADC_boards[i], TRANSDUCER_OUTPUT_TYPE[i],
                           HRADC_HEATER_ENABLE[i], HRADC_MONITOR_ENABLE[i]);
    }

    Config_HRADC_SoC(HRADC_FREQ_SAMP);

    /// Initialization of PWM modules
    g_pwm_modules.num_modules = 7;

    PWM_MODULATOR_1 = &EPwm1Regs;           // Master PWM1 - Main PWM
    PWM_MODULATOR_2 = &EPwm2Regs;           // Slave to PWM1 - Main PWM
    PWM_MODULATOR_3 = &EPwm3Regs;           // Slave to PWM1 - Clamp PWM
    PWM_MODULATOR_4 = &EPwm4Regs;           // Slave to PWM1 - Clamp PWM
    PWM_MODULATOR_5 = &EPwm5Regs;           // Slave to PWM1 - Active Rectifier PWM
    PWM_MODULATOR_6 = &EPwm6Regs;           // Slave to PWM1 - Active Rectifier PWM
    PWM_ISR_CONTROLLER = &EPwm7Regs;        // Control PWM

    disable_pwm_outputs();
    disable_pwm_tbclk();
    init_pwm_mep_sfo();

    /// PWM initialization

    // init_pwm_module(PWM module, freq, specified primary module, synchronization, 
    // phase between modules[º], channel B configuration [`PWM_ChB_Independent`,
    // PWM_ChB_Complementary`], dead-time between channel A and B [ns])

    // MAIN SWITCHES
    init_pwm_module(PWM_MODULATOR_1, PWM_FREQ, 0, PWM_Sync_Master, 0,
                    PWM_ChB_Complementary, PWM_DEAD_TIME);
    init_pwm_module(PWM_MODULATOR_2, PWM_FREQ, 1, PWM_Sync_Slave, 180,
                    PWM_ChB_Complementary, PWM_DEAD_TIME);

    set_pwm_deadtime_edge(PWM_MODULATOR_1, PWM_DEAD_TIME_RISING, PWM_DEAD_TIME_FALLING);
    set_pwm_deadtime_edge(PWM_MODULATOR_2, PWM_DEAD_TIME_RISING, PWM_DEAD_TIME_FALLING);

    // CLAMP SWITCHES
    init_pwm_module(PWM_MODULATOR_3, PWM_FREQ, 0, PWM_Sync_Slave, 0,
                    PWM_ChB_Complementary, PWM_DEAD_TIME);
    init_pwm_module(PWM_MODULATOR_4, PWM_FREQ, 3, PWM_Sync_Slave, 180,
                    PWM_ChB_Complementary, PWM_DEAD_TIME);

    set_pwm_deadtime_edge(PWM_MODULATOR_3, PWM_DEAD_TIME_RISING, PWM_DEAD_TIME_FALLING);
    set_pwm_deadtime_edge(PWM_MODULATOR_4, PWM_DEAD_TIME_RISING, PWM_DEAD_TIME_FALLING);

    // ACTIVE RECTTIFIER SWITCHES
    init_pwm_module(PWM_MODULATOR_5, PWM_FREQ, 0, PWM_Sync_Slave, 0,
                    PWM_ChB_Complementary, PWM_DEAD_TIME);
    init_pwm_module(PWM_MODULATOR_6, PWM_FREQ, 5, PWM_Sync_Slave, 180,
                    PWM_ChB_Complementary, PWM_DEAD_TIME);

    set_pwm_deadtime_edge(PWM_MODULATOR_5, PWM_DEAD_TIME_RISING_ACT_RCT, PWM_DEAD_TIME_FALLING_ACT_RCT);
    set_pwm_deadtime_edge(PWM_MODULATOR_6, PWM_DEAD_TIME_RISING_ACT_RCT, PWM_DEAD_TIME_FALLING_ACT_RCT);

    /// Fix turn-on time
    duty_cycle = (((T_ON_US*1e-6) + (PWM_DEAD_TIME_RISING*1e-9))*PWM_FREQ);

    set_pwm_duty_chA(PWM_MODULATOR_1, duty_cycle);
    set_pwm_duty_chA(PWM_MODULATOR_2, duty_cycle);
    set_pwm_duty_chA(PWM_MODULATOR_3, duty_cycle);
    set_pwm_duty_chA(PWM_MODULATOR_4, duty_cycle);
    set_pwm_duty_chA(PWM_MODULATOR_5, duty_cycle);
    set_pwm_duty_chA(PWM_MODULATOR_6, duty_cycle);

    /// Set time-base period register on shadow mode (update on next cycle)
    PWM_MODULATOR_1->TBCTL.bit.PRDLD = TB_SHADOW;
    PWM_MODULATOR_2->TBCTL.bit.PRDLD = TB_SHADOW;
    PWM_MODULATOR_3->TBCTL.bit.PRDLD = TB_SHADOW;
    PWM_MODULATOR_4->TBCTL.bit.PRDLD = TB_SHADOW;
    PWM_MODULATOR_5->TBCTL.bit.PRDLD = TB_SHADOW;
    PWM_MODULATOR_6->TBCTL.bit.PRDLD = TB_SHADOW;

    // This setting allows large frequency steps to happen without error
    // PWM_MODULATOR_2->TBCTL2.bit.PRDLDSYNC = 0x01;
    // PWM_MODULATOR_3->TBCTL2.bit.PRDLDSYNC = 0x02;
    // PWM_MODULATOR_4->TBCTL2.bit.PRDLDSYNC = 0x03;
    // PWM_MODULATOR_5->TBCTL2.bit.PRDLDSYNC = 0x04;
    // PWM_MODULATOR_6->TBCTL2.bit.PRDLDSYNC = 0x05;

    // Changing from PWM to FSM
    FREQ_MODULATED = PWM_FREQ;

    /// PWM module for ISR controller
    init_pwm_module(PWM_ISR_CONTROLLER, ISR_CONTROL_FREQ, 0, PWM_Sync_Master, 0,
                    PWM_ChB_Independent, PWM_DEAD_TIME);
    set_pwm_duty_chA(PWM_ISR_CONTROLLER, 0.5);

    InitEPwm1Gpio();
    InitEPwm2Gpio();
    InitEPwm3Gpio();
    InitEPwm4Gpio();
    InitEPwm5Gpio();
    InitEPwm6Gpio();

    /// Initialization of timers
    InitCpuTimers();
    ConfigCpuTimer(&CpuTimer0, C28_FREQ_MHZ, 1000000);
    CpuTimer0Regs.TCR.bit.TIE = 0;
}

static void term_peripherals_drivers(void)
{
}

static void init_controller(void)
{
    /**
     * TODO: initialize WfmRef and Samples Buffer
     */

    init_ps_module(&g_ipc_ctom.ps_module[0],
                   g_ipc_mtoc.ps_module[0].ps_status.bit.model,
                   &turn_on, &turn_off, &isr_soft_interlock,
                   &isr_hard_interlock, &reset_interlocks);

    g_ipc_ctom.ps_module[1].ps_status.all = 0;
    g_ipc_ctom.ps_module[2].ps_status.all = 0;
    g_ipc_ctom.ps_module[3].ps_status.all = 0;

    init_event_manager(0, ISR_CONTROL_FREQ,
                       NUM_HARD_INTERLOCKS, NUM_SOFT_INTERLOCKS,
                       &HARD_INTERLOCKS_DEBOUNCE_TIME,
                       &HARD_INTERLOCKS_RESET_TIME,
                       &SOFT_INTERLOCKS_DEBOUNCE_TIME,
                       &SOFT_INTERLOCKS_RESET_TIME);

    init_control_framework(&g_controller_ctom);

    init_ipc();

    init_wfmref(&WFMREF, WFMREF_SELECTED_PARAM[0], WFMREF_SYNC_MODE_PARAM[0],
                ISR_CONTROL_FREQ, WFMREF_FREQUENCY_PARAM[0], WFMREF_GAIN_PARAM[0],
                WFMREF_OFFSET_PARAM[0], &g_wfmref_data.data, SIZE_WFMREF,
                &I_LOAD_REFERENCE);

    /***********************************************/
    /** INITIALIZATION OF SIGNAL GENERATOR MODULE **/
    /***********************************************/

    disable_siggen(&SIGGEN);

    init_siggen(&SIGGEN, ISR_CONTROL_FREQ, &I_LOAD_REFERENCE);

    cfg_siggen(&SIGGEN, SIGGEN_TYPE_PARAM, SIGGEN_NUM_CYCLES_PARAM,
               SIGGEN_FREQ_PARAM, SIGGEN_AMP_PARAM,
               SIGGEN_OFFSET_PARAM, SIGGEN_AUX_PARAM);

    /**
     *        name:     SRLIM_SIGGEN_AMP
     * description:     Signal generator amplitude slew-rate limiter
     *    DP class:     DSP_SRLim
     *          in:     SIGGEN_MTOC[0].amplitude
     *         out:     SIGGEN_CTOM[0].amplitude
     */

    init_dsp_srlim(SRLIM_SIGGEN_AMP, MAX_SLEWRATE_SIGGEN_AMP, ISR_CONTROL_FREQ,
                   &SIGGEN_MTOC[0].amplitude, &SIGGEN.amplitude);

    /**
     *        name:     SRLIM_SIGGEN_OFFSET
     * description:     Signal generator offset slew-rate limiter
     *    DP class:     DSP_SRLim
     *          in:     SIGGEN_MTOC[0].offset
     *         out:     SIGGEN_CTOM[0].offset
     */

    init_dsp_srlim(SRLIM_SIGGEN_OFFSET, MAX_SLEWRATE_SIGGEN_OFFSET,
                   ISR_CONTROL_FREQ, &SIGGEN_MTOC[0].offset,
                   &SIGGEN_CTOM[0].offset);

    /*************************************************/
    /** INITIALIZATION OF LOAD CURRENT CONTROL LOOP **/
    /*************************************************/

    /**
     *        name:     SRLIM_I_LOAD_REFERENCE
     * description:     Load current slew-rate limiter
     *    DP class:     DSP_SRLim
     *          in:     I_LOAD_SETPOINT
     *         out:     I_LOAD_REFERENCE
     */

    init_dsp_srlim(SRLIM_I_LOAD_REFERENCE, MAX_SLEWRATE_SLOWREF, ISR_CONTROL_FREQ,
                   &I_LOAD_SETPOINT, &I_LOAD_REFERENCE);

    /**
     *        name:     ERROR_I_LOAD
     * description:     Load current reference error
     *  dsp module:     DSP_Error
     *           +:     I_LOAD_REFERENCE
     *           -:     I_LOAD_MEAN
     *         out:     I_LOAD_ERROR
     */

    init_dsp_error(ERROR_I_LOAD, &I_LOAD_REFERENCE, &I_LOAD_MEAN, &I_LOAD_ERROR);

    /**
     *        name:     PI_CONTROLLER_I_LOAD
     * description:     Load current PI controller
     *  dsp module:     DSP_PI
     *          in:     I_LOAD_ERROR
     *         out:     FREQ_MODULATED
     */

    init_dsp_pi(PI_CONTROLLER_I_LOAD, KP_I_LOAD, KI_I_LOAD, ISR_CONTROL_FREQ,
                MAX_REF_CL, MIN_REF_CL, &I_LOAD_ERROR, &FREQ_MODULATED);

    /***************************************************/
    /** INITIALIZATION OF DC-LINK VOLTAGE FEEDFORWARD **/
    /***************************************************/

    /**
     *        name:     FF_V_DCLINK
     * description:     DCLINK voltage feed-forward controller
     *    DP class:     DSP_FF
     *    vdc_meas:     V_DCLINK
     *          in:     FREQ_MODULATED_COMPENS
     *         out:     FREQ_MODULATED_FF
     */

    init_dsp_vdclink_ff(FF_V_DCLINK, NOM_V_DCLINK_FF,
                        MIN_V_DCLINK_FF,
                        &V_DCLINK, &FREQ_MODULATED_COMPENS,
                        &FREQ_MODULATED_FF);

    /******************************/
    /** INITIALIZATION OF SCOPES **/
    /******************************/

    init_scope(&SCOPE, ISR_CONTROL_FREQ, SCOPE_FREQ_SAMPLING_PARAM[0],
               &g_buf_samples_ctom[0], SIZE_BUF_SAMPLES_CTOM,
               SCOPE_SOURCE_PARAM[0], &run_scope_shared_ram);

    /**
     * Reset all internal variables
     */
    reset_controller();
}

/**
 * Reset all internal variables from controller
 */
static void reset_controller(void)
{
    set_pwm_freq(PWM_MODULATOR_1, PWM_FREQ);
    set_pwm_freq(PWM_MODULATOR_2, PWM_FREQ);

    set_pwm_freq(PWM_MODULATOR_3, PWM_FREQ);
    set_pwm_freq(PWM_MODULATOR_4, PWM_FREQ);

    set_pwm_freq(PWM_MODULATOR_5, PWM_FREQ);
    set_pwm_freq(PWM_MODULATOR_6, PWM_FREQ);

    cfg_pwm_sync(PWM_MODULATOR_1, PWM_Sync_Master, 0.0);
    cfg_pwm_sync(PWM_MODULATOR_2, PWM_Sync_Slave, 180.0);

    cfg_pwm_sync(PWM_MODULATOR_3, PWM_Sync_Slave, 0.0);
    cfg_pwm_sync(PWM_MODULATOR_4, PWM_Sync_Slave, 180.0);

    cfg_pwm_sync(PWM_MODULATOR_5, PWM_Sync_Slave, 0.0);
    cfg_pwm_sync(PWM_MODULATOR_6, PWM_Sync_Slave, 180.0);

    g_ipc_ctom.ps_module[0].ps_status.bit.openloop = LOOP_STATE;

    /// Change in the default reference according to loop-state:
    if(LOOP_STATE == CLOSED_LOOP)
    {
        I_LOAD_SETPOINT = MIN_REF[0];
        I_LOAD_REFERENCE = MIN_REF[0];
    }
    else
    {
        I_LOAD_SETPOINT = MIN_REF_OL[0];
        I_LOAD_REFERENCE = MIN_REF_OL[0];
    }

    reset_dsp_srlim(SRLIM_I_LOAD_REFERENCE);
    reset_dsp_error(ERROR_I_LOAD);
    reset_dsp_pi(PI_CONTROLLER_I_LOAD);
    reset_dsp_vdclink_ff(FF_V_DCLINK);
    reset_dsp_srlim(SRLIM_SIGGEN_AMP);
    reset_dsp_srlim(SRLIM_SIGGEN_OFFSET);
    disable_siggen(&SIGGEN);

    reset_wfmref(&WFMREF);
}

/**
 * Enable control ISR
 */
static void enable_controller()
{
    stop_DMA();
    DELAY_US(5);
    start_DMA();
    HRADCs_Info.enable_Sampling = 1;
    enable_pwm_tbclk();
}

/**
 * Disable control ISR
 */
static void disable_controller()
{
    disable_pwm_tbclk();
    HRADCs_Info.enable_Sampling = 0;
    stop_DMA();

    reset_controller();
}

/**
 * ISR for control initialization
 */
static interrupt void isr_init_controller(void)
{
    EALLOW;
    PieVectTable.EPWM3_INT = &isr_controller;
    EDIS;

    PWM_ISR_CONTROLLER->ETSEL.bit.INTSEL = ET_CTR_ZERO;
    PWM_ISR_CONTROLLER->ETCLR.bit.INT = 1;

    /**
     *  Enable XINT2 (external interrupt 2) interrupt used for sync pulses for
     *  the first time
     *
     *  TODO: include here mechanism described in section 1.5.4.3 from F28M36
     *  Technical Reference Manual (SPRUHE8E) to clear flag before enabling, to
     *  avoid false alarms that may occur when sync pulses are received during
     *  firmware initialization.
     */
    PieCtrlRegs.PIEIER1.bit.INTx5 = 1;

    /// Clear interrupt flag for PWM interrupts group
    PieCtrlRegs.PIEACK.all |= M_INT3;
}

/**
 * Control ISR
 */
static interrupt void isr_controller(void)
{
    static float temp[4];
    static uint16_t i;

    //SET_DEBUG_GPIO0;
    SET_DEBUG_GPIO1;

    temp[0] = 0.0;
    temp[1] = 0.0;
    temp[2] = 0.0;
    temp[3] = 0.0;

    /// Get HRADC samples
    for(i = 0; i < decimation_factor; i++)
    {
        temp[0] += (float) *(HRADCs_Info.HRADC_boards[0].SamplesBuffer++);
        temp[1] += (float) *(HRADCs_Info.HRADC_boards[1].SamplesBuffer++);
        temp[2] += (float) *(HRADCs_Info.HRADC_boards[2].SamplesBuffer++);
        temp[3] += (float) *(HRADCs_Info.HRADC_boards[3].SamplesBuffer++);
    }

    //CLEAR_DEBUG_GPIO1;

    HRADCs_Info.HRADC_boards[0].SamplesBuffer = buffers_HRADC[0];
    HRADCs_Info.HRADC_boards[1].SamplesBuffer = buffers_HRADC[1];
    HRADCs_Info.HRADC_boards[2].SamplesBuffer = buffers_HRADC[2];
    HRADCs_Info.HRADC_boards[3].SamplesBuffer = buffers_HRADC[3];

    temp[0] *= HRADCs_Info.HRADC_boards[0].gain * decimation_coeff;
    temp[0] += HRADCs_Info.HRADC_boards[0].offset;

    temp[1] *= HRADCs_Info.HRADC_boards[1].gain * decimation_coeff;
    temp[1] += HRADCs_Info.HRADC_boards[1].offset;

    temp[2] *= HRADCs_Info.HRADC_boards[2].gain * decimation_coeff;
    temp[2] += HRADCs_Info.HRADC_boards[2].offset;

    temp[3] *= HRADCs_Info.HRADC_boards[3].gain * decimation_coeff;
    temp[3] += HRADCs_Info.HRADC_boards[3].offset;

    if(NUM_DCCTs)
    {
        I_LOAD_1 = temp[0] - TRANSDUCER_OFFSET[0];
        I_LOAD_2 = temp[1] - TRANSDUCER_OFFSET[1];
        V_DCLINK = temp[2];

        I_LOAD_MEAN = 0.5*(I_LOAD_1 + I_LOAD_2);
        I_LOAD_DIFF = I_LOAD_1 - I_LOAD_2;
    }
    else
    {
        I_LOAD_1 = temp[0];
        V_DCLINK = temp[1];

        I_LOAD_MEAN = I_LOAD_1 - TRANSDUCER_OFFSET[0];
        I_LOAD_DIFF = 0;
    }

    /// Check whether power supply is ON
    if(g_ipc_ctom.ps_module[0].ps_status.bit.state >= SlowRef)
    {
        /// Calculate reference according to operation mode
        switch(g_ipc_ctom.ps_module[0].ps_status.bit.state)
        {
            case SlowRef:
            case SlowRefSync:
            {
                run_dsp_srlim(SRLIM_I_LOAD_REFERENCE, USE_MODULE);
                break;
            }
            case Cycle:
            {
                run_dsp_srlim(SRLIM_SIGGEN_AMP, USE_MODULE);
                run_dsp_srlim(SRLIM_SIGGEN_OFFSET, USE_MODULE);
                SIGGEN.p_run_siggen(&SIGGEN);
                break;
            }
            case RmpWfm:
            case MigWfm:
            {
                run_wfmref(&WFMREF);
                break;
            }
            default:
            {
                break;
            }
        }

        /// Open-loop
        if(g_ipc_ctom.ps_module[0].ps_status.bit.openloop)
        {
            SATURATE(I_LOAD_REFERENCE, MAX_REF_OL[0], MIN_REF_OL[0])
			FREQ_MODULATED_FF = I_LOAD_REFERENCE;
            SATURATE(FREQ_MODULATED_FF, MAX_REF_OL[0], MIN_REF_OL[0]);
        }
        /// Closed-loop
        else
        {
            SATURATE(I_LOAD_REFERENCE, MAX_REF[0], MIN_REF[0]);
            run_dsp_error(ERROR_I_LOAD);
            run_dsp_pi(PI_CONTROLLER_I_LOAD);

            /// Modulation frequency dead-zone compensation
            FREQ_MODULATED_COMPENS = FREQ_MODULATED + FREQ_DEADZONE_HZ;
            SATURATE(FREQ_MODULATED_COMPENS, MAX_REF_CL, MIN_REF_CL);

            run_dsp_vdclink_ff(FF_V_DCLINK);
            SATURATE(FREQ_MODULATED_FF, MAX_REF_CL, MIN_REF_CL);
        }

        set_pwm_freq(PWM_MODULATOR_1, FREQ_MODULATED_FF);
        set_pwm_freq(PWM_MODULATOR_2, FREQ_MODULATED_FF);
        set_pwm_freq(PWM_MODULATOR_3, FREQ_MODULATED_FF);
        set_pwm_freq(PWM_MODULATOR_4, FREQ_MODULATED_FF);
        set_pwm_freq(PWM_MODULATOR_5, FREQ_MODULATED_FF);
        set_pwm_freq(PWM_MODULATOR_6, FREQ_MODULATED_FF);

        cfg_pwm_sync(PWM_MODULATOR_1, PWM_Sync_Master, 0.0);
        cfg_pwm_sync(PWM_MODULATOR_2, PWM_Sync_Slave, 180.0);
        cfg_pwm_sync(PWM_MODULATOR_3, PWM_Sync_Slave, 0.0);
        cfg_pwm_sync(PWM_MODULATOR_4, PWM_Sync_Slave, 180.0);
        cfg_pwm_sync(PWM_MODULATOR_5, PWM_Sync_Slave, 0.0);
        cfg_pwm_sync(PWM_MODULATOR_6, PWM_Sync_Slave, 180.0);
    }

    RUN_SCOPE(SCOPE);

    SET_INTERLOCKS_TIMEBASE_FLAG(0);

    /**
     * Re-enable external interrupt 2 (XINT2) interrupts to allow sync pulses to
     * be handled once per isr_controller
     */
    if(PieCtrlRegs.PIEIER1.bit.INTx5 == 0)
    {
        /// Set alarm if counter is below limit when receiving new sync pulse
        if(counter_sync_period < MIN_NUM_ISR_CONTROLLER_SYNC)
        {
            g_ipc_ctom.ps_module[0].ps_alarms = High_Sync_Input_Frequency;
        }

        /// Store counter value on BSMP variable
        g_ipc_ctom.period_sync_pulse = counter_sync_period;
        counter_sync_period = 0;
    }

    counter_sync_period++;

    /**
     * Reset counter to threshold to avoid false alarms during its overflow
     */
    if(counter_sync_period == MAX_NUM_ISR_CONTROLLER_SYNC)
    {
        counter_sync_period = MIN_NUM_ISR_CONTROLLER_SYNC;
    }

    /// Re-enable XINT2 (external interrupt 2) interrupt used for sync pulses
    PieCtrlRegs.PIEIER1.bit.INTx5 = 1;

    /// Clear interrupt flags for PWM interrupts
    //PWM_MODULATOR_1->ETCLR.bit.INT = 1;
    //PWM_MODULATOR_2->ETCLR.bit.INT = 1;
    PWM_ISR_CONTROLLER->ETCLR.bit.INT = 1;
    PieCtrlRegs.PIEACK.all |= M_INT3;

    CLEAR_DEBUG_GPIO1;
}

/**
 * Initialization of interruptions.
 */
static void init_interruptions(void)
{
    EALLOW;
    //PieVectTable.EPWM3_INT =  &isr_init_controller;
    PieVectTable.EPWM7_INT =  &isr_init_controller;
    EDIS;

    //PieCtrlRegs.PIEIER3.bit.INTx3 = 1;
    PieCtrlRegs.PIEIER7.bit.INTx7 = 1;

    //enable_pwm_interrupt(PWM_MODULATOR_1);
    //enable_pwm_interrupt(PWM_MODULATOR_2);
    enable_pwm_interrupt(PWM_ISR_CONTROLLER);

    IER |= M_INT1;
    IER |= M_INT3;
    IER |= M_INT11;

    /// Enable global interrupts (EINT)
    EINT;
    ERTM;
}

/**
 * Termination of interruptions.
 */
static void term_interruptions(void)
{
    /// Disable global interrupts (EINT)
    DINT;
    DRTM;

    /// Clear enables
    IER = 0;
    //PieCtrlRegs.PIEIER3.bit.INTx1 = 0;  /// ePWM1
    //PieCtrlRegs.PIEIER3.bit.INTx3 = 0;  /// ePWM3
    PieCtrlRegs.PIEIER7.bit.INTx7 = 0;  /// ePWM7

    //disable_pwm_interrupt(PWM_MODULATOR_1);
    //disable_pwm_interrupt(PWM_MODULATOR_2);
    // All PWMs are enable
    disable_pwm_interrupt(PWM_ISR_CONTROLLER);

    /// Clear flags
    PieCtrlRegs.PIEACK.all |= M_INT1 | M_INT3 | M_INT11;
}

/**
 * Turn power supply on.
 *
 * @param dummy dummy argument due to ps_module pointer
 */
static void turn_on(uint16_t dummy)
{
    #ifdef USE_ITLK
    if(g_ipc_ctom.ps_module[0].ps_status.bit.state == Off)
    #else
    if(g_ipc_ctom.ps_module[0].ps_status.bit.state <= Interlock)
    #endif
    {
        /*if(V_DCLINK > MAX_V_DCLINK_TURN_ON)
        {
            BYPASS_HARD_INTERLOCK_DEBOUNCE(0, DCLink_Overvoltage);
            set_hard_interlock(0, DCLink_Overvoltage);
        }*/

        #ifdef USE_ITLK
        else
        {
        #endif

            /*if(PIN_STATUS_CONTACTOR_K2)
            {
                BYPASS_HARD_INTERLOCK_DEBOUNCE(0, Welded_Contactor_K2_Fault);
                set_hard_interlock(0, Welded_Contactor_K2_Fault);
            }*/

            PIN_CLOSE_CONTACTOR_K1;
            DELAY_US(TIMEOUT_CONTACTOR_K1_CLOSED_MS*1000);

            /*if(!PIN_STATUS_CONTACTOR_K1)
            {
                BYPASS_HARD_INTERLOCK_DEBOUNCE(0, Opened_Contactor_K1_Fault);
                set_hard_interlock(0, Opened_Contactor_K1_Fault);
            }*/

            #ifdef USE_ITLK
            else
            {
            #endif
                g_ipc_ctom.ps_module[0].ps_status.bit.state = Initializing;
            #ifdef USE_ITLK
            }
        }
        #endif
    }
}

/**
 * Turn off specified power supply.
 *
 * @param dummy dummy argument due to ps_module pointer
 */
static void turn_off(uint16_t dummy)
{
    // Disable all PWMs before star. Only the control ref ISR is on
    disable_pwm_output(0);
    disable_pwm_output(1);
    disable_pwm_output(2);
    disable_pwm_output(3);
    disable_pwm_output(4);
    disable_pwm_output(5);
    disable_pwm_output(6);

    PIN_OPEN_CONTACTOR_K1;
    DELAY_US(TIMEOUT_CONTACTOR_K1_OPENED_MS*1000);

    reset_controller();

    if(g_ipc_ctom.ps_module[0].ps_status.bit.state != Interlock)
    {
        g_ipc_ctom.ps_module[0].ps_status.bit.state = Off;
    }
}

/**
 * Reset interlocks for specified power supply.
 *
 * @param dummy dummy argument due to ps_module pointer
 */
static void reset_interlocks(uint16_t dummy)
{
    g_ipc_ctom.ps_module[0].ps_hard_interlock = 0;
    g_ipc_ctom.ps_module[0].ps_soft_interlock = 0;
    g_ipc_ctom.ps_module[0].ps_alarms = 0;

    if(g_ipc_ctom.ps_module[0].ps_status.bit.state < Initializing)
    {
    	init_control_framework(&g_controller_ctom);
    	init_control_framework(&g_controller_mtoc);

    	if(PIN_STATUS_CONTACTOR_K1)
        {
            PIN_CLOSE_CONTACTOR_K1;
            DELAY_US(RESET_PULSE_TIME_CONTACTOR_K1_MS*1000);
            PIN_OPEN_CONTACTOR_K1;
            DELAY_US(TIMEOUT_CONTACTOR_K1_OPENED_MS*1000);
        }

        g_ipc_ctom.ps_module[0].ps_status.bit.state = Off;
    }
}

/**
 * Check interlocks of this specific power supply topology
 */

 
static inline void check_interlocks(void)
{
    //SET_DEBUG_GPIO1;

    /*if(fabs(I_LOAD_MEAN) > MAX_ILOAD)
    {
        set_hard_interlock(0, Load_Overcurrent);
    }

    if(fabs(I_LOAD_DIFF) > MAX_DCCTS_DIFF)
    {
        set_soft_interlock(0, DCCT_High_Difference);
    }

    if(V_DCLINK > MAX_V_DCLINK)
    {
        set_hard_interlock(0, DCLink_Overvoltage);
    }

    if(!PIN_STATUS_DCCT_1_STATUS)
    {
        set_soft_interlock(0, DCCT_1_Fault);
    }

    if(NUM_DCCTs && !PIN_STATUS_DCCT_2_STATUS)
    {
        set_soft_interlock(0, DCCT_2_Fault);
    }

    if(PIN_STATUS_DCCT_1_ACTIVE)
    {
    	if(fabs(I_LOAD_1) < MIN_I_ACTIVE_DCCT)
    	{
    		set_soft_interlock(0, Load_Feedback_1_Fault);
    	}
    }
    else
    {
    	if(fabs(I_LOAD_1) > MAX_I_IDLE_DCCT)
    	{
    		set_soft_interlock(0, Load_Feedback_1_Fault);
    	}
    }

    if(NUM_DCCTs)
    {
    	if(PIN_STATUS_DCCT_2_ACTIVE)
    	{
    		if(fabs(I_LOAD_2) < MIN_I_ACTIVE_DCCT)
    		{
    			set_soft_interlock(0, Load_Feedback_2_Fault);
    		}
    	}
    	else
    	{
    		if(fabs(I_LOAD_2) > MAX_I_IDLE_DCCT)
    		{
    			set_soft_interlock(0, Load_Feedback_2_Fault);
    		}
    	}
    }

    if(!PIN_STATUS_EXTERNAL_INTERLOCK)
    {
    	set_hard_interlock(0, External_Itlk);
    }*/

    DINT;

    if(g_ipc_ctom.ps_module[0].ps_status.bit.state <= Interlock)
    {
        /*if(PIN_STATUS_CONTACTOR_K1)
        {
            set_hard_interlock(0, Welded_Contactor_K1_Fault);
        }

        if(PIN_STATUS_CONTACTOR_K2)
        {
            set_hard_interlock(0, Welded_Contactor_K2_Fault);
        }*/
    }

    else
    {
        /*if(!PIN_STATUS_CONTACTOR_K1)
        {
            set_hard_interlock(0, Opened_Contactor_K1_Fault);
        }*/

        /*if(!PIN_STATUS_CONTACTOR_K2 && !PIN_STATUS_CONTACTOR_K1)
        {
            set_hard_interlock(0, Opened_Contactor_K2_Fault);
        }*/

        if(g_ipc_ctom.ps_module[0].ps_status.bit.state == Initializing)
        {
            if(V_DCLINK > MIN_V_DCLINK)
            {
                // After checking all the interlocks, the pwms may be enable
                g_ipc_ctom.ps_module[0].ps_status.bit.state = SlowRef;
                enable_pwm_output(0);
                enable_pwm_output(1);
                enable_pwm_output(2);
                enable_pwm_output(3);
                enable_pwm_output(4);
                enable_pwm_output(5);
                enable_pwm_output(6);
            }
        }

        else if(g_ipc_ctom.ps_module[0].ps_status.bit.state > Initializing) 
        /// Power supply ON
        {
            if(V_DCLINK < MIN_V_DCLINK)
            {
                set_hard_interlock(0, DCLink_Undervoltage);
            }
        }
    }

    EINT;
    
    run_interlocks_debouncing(0);
    
    #ifdef USE_ITLK
    if(g_ipc_ctom.ps_module[0].ps_status.bit.state == Interlock)
    #else
    if(g_ipc_ctom.ps_module[0].ps_hard_interlock || g_ipc_ctom.ps_module[0].ps_soft_interlock)
    #endif
    
    //CLEAR_DEBUG_GPIO1;
}
