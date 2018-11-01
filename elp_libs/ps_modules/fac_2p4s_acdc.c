/******************************************************************************
 * Copyright (C) 2018 by LNLS - Brazilian Synchrotron Light Laboratory
 *
 * Redistribution, modification or use of this software in source or binary
 * forms is permitted as long as the files maintain this copyright. LNLS and
 * the Brazilian Center for Research in Energy and Materials (CNPEM) are not
 * liable for any misuse of this material.
 *
 *****************************************************************************/

/**
 * @file fac_2p4s_acdc.c
 * @brief FAC-2P4S AC/DC Stage module
 * 
 * Module for control of two AC/DC modules of FAC power supplies for dipoles
 * from booster. It implements the individual controllers for input current and
 * capacitor bank voltage of each AC/DC module.
 *
 * @author gabriel.brunheira
 * @date 21/07/2018
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
#include "udc_net/udc_net.h"

#include "fac_2p4s_acdc.h"

/**
 * PWM parameters
 */
#define PWM_FREQ                g_ipc_mtoc.pwm.freq_pwm
#define PWM_DEAD_TIME           g_ipc_mtoc.pwm.dead_time
#define PWM_MAX_DUTY            g_ipc_mtoc.pwm.max_duty
#define PWM_MIN_DUTY            g_ipc_mtoc.pwm.min_duty
#define PWM_MAX_DUTY_OL         g_ipc_mtoc.pwm.max_duty_openloop
#define PWM_MIN_DUTY_OL         g_ipc_mtoc.pwm.min_duty_openloop

/**
 * Control parameters
 */
#define MAX_REF                 g_ipc_mtoc.control.max_ref
#define MIN_REF                 g_ipc_mtoc.control.min_ref
#define MAX_REF_OL              g_ipc_mtoc.control.max_ref_openloop
#define MIN_REF_OL              g_ipc_mtoc.control.min_ref_openloop
#define MAX_REF_SLEWRATE        g_ipc_mtoc.control.slewrate_slowref
#define MAX_SR_SIGGEN_OFFSET    g_ipc_mtoc.control.slewrate_siggen_offset
#define MAX_SR_SIGGEN_AMP       g_ipc_mtoc.control.slewrate_siggen_amp

#define ISR_CONTROL_FREQ        g_ipc_mtoc.control.freq_isr_control

#define HRADC_FREQ_SAMP         g_ipc_mtoc.hradc.freq_hradc_sampling
#define HRADC_SPI_CLK           g_ipc_mtoc.hradc.freq_spiclk
#define NUM_HRADC_BOARDS        g_ipc_mtoc.hradc.num_hradc

#define TIMESLICER_BUFFER       1
#define BUFFER_FREQ             g_ipc_mtoc.control.freq_timeslicer[TIMESLICER_BUFFER]
#define BUFFER_DECIMATION       (uint16_t) roundf(ISR_CONTROL_FREQ / BUFFER_FREQ)

#define TIMESLICER_CONTROLLER   2
#define CONTROLLER_FREQ_SAMP    g_ipc_mtoc.control.freq_timeslicer[TIMESLICER_CONTROLLER]
#define CONTROLLER_DECIMATION   (uint16_t) roundf(ISR_CONTROL_FREQ / CONTROLLER_FREQ_SAMP)

#define SIGGEN                  g_ipc_ctom.siggen

/**
 * HRADC parameters
 */
#define HRADC_HEATER_ENABLE     g_ipc_mtoc.hradc.enable_heater
#define HRADC_MONITOR_ENABLE    g_ipc_mtoc.hradc.enable_monitor
#define TRANSDUCER_OUTPUT_TYPE  g_ipc_mtoc.hradc.type_transducer_output
#if (HRADC_v2_0)
    #define TRANSDUCER_GAIN     -g_ipc_mtoc.hradc.gain_transducer
#endif
#if (HRADC_v2_1)
    #define TRANSDUCER_GAIN     g_ipc_mtoc.hradc.gain_transducer
#endif

/**
 * Analog variables parameters
 */
#define MAX_V_CAPBANK           g_ipc_mtoc.analog_vars.max[0]

#define MAX_VOUT_RECT           g_ipc_mtoc.analog_vars.max[1]
#define MAX_IOUT_RECT           g_ipc_mtoc.analog_vars.max[2]
#define MAX_IOUT_RECT_REF       g_ipc_mtoc.analog_vars.max[3]
#define MIN_IOUT_RECT_REF       g_ipc_mtoc.analog_vars.min[3]

#define MAX_TEMP_HEATSINK       g_ipc_mtoc.analog_vars.max[4]
#define MAX_TEMP_INDUCTORS      g_ipc_mtoc.analog_vars.max[5]

#define TIMEOUT_AC_MAINS_CONTACTOR_CLOSED_MS   g_ipc_mtoc.analog_vars.max[6]
#define TIMEOUT_AC_MAINS_CONTACTOR_OPENED_MS   g_ipc_mtoc.analog_vars.max[7]

#define NETSIGNAL_ELEM_CTOM_BUF     g_ipc_mtoc.analog_vars.max[8]
#define NETSIGNAL_ELEM_MTOC_BUF     g_ipc_mtoc.analog_vars.min[8]

#define NETSIGNAL_CTOM_BUF      g_controller_ctom.net_signals[(uint16_t) NETSIGNAL_ELEM_CTOM_BUF].f
#define NETSIGNAL_MTOC_BUF      g_controller_mtoc.net_signals[(uint16_t) NETSIGNAL_ELEM_MTOC_BUF].f

/**
 * Shared defines between both modules
 */
#define V_CAPBANK_SETPOINT              g_ipc_ctom.ps_module[0].ps_setpoint
#define V_CAPBANK_REFERENCE             g_ipc_ctom.ps_module[0].ps_reference

#define SRLIM_V_CAPBANK_REFERENCE       &g_controller_ctom.dsp_modules.dsp_srlim[0]

#define SRLIM_SIGGEN_AMP                &g_controller_ctom.dsp_modules.dsp_srlim[1]
#define SRLIM_SIGGEN_OFFSET             &g_controller_ctom.dsp_modules.dsp_srlim[2]

#define NF_ALPHA                        0.99

#define BUF_SAMPLES                     &g_ipc_ctom.buf_samples[0]

/**
 * Defines for AC/DC Module A
 */
#define MOD_A_ID        0x0

#define PIN_OPEN_AC_MAINS_CONTACTOR_MOD_A       CLEAR_GPDO1;
#define PIN_CLOSE_AC_MAINS_CONTACTOR_MOD_A      SET_GPDO1;
#define PIN_STATUS_AC_MAINS_CONTACTOR_MOD_A     GET_GPDI5

#define V_CAPBANK_MOD_A                 g_controller_ctom.net_signals[0].f  // HRADC0
#define IOUT_RECT_MOD_A                 g_controller_ctom.net_signals[1].f  // HRADC1

#define VOUT_RECT_MOD_A                 g_controller_mtoc.net_signals[0].f
#define TEMP_HEATSINK_MOD_A             g_controller_mtoc.net_signals[1].f
#define TEMP_INDUCTORS_MOD_A            g_controller_mtoc.net_signals[2].f

#define DUTY_CYCLE_MOD_A                g_controller_ctom.output_signals[0].f

#define ERROR_V_CAPBANK_MOD_A           &g_controller_ctom.dsp_modules.dsp_error[0]

#define PI_CONTROLLER_V_CAPBANK_MOD_A           &g_controller_ctom.dsp_modules.dsp_pi[0]
#define PI_CONTROLLER_V_CAPBANK_MOD_A_COEFFS    g_controller_mtoc.dsp_modules.dsp_pi[0].coeffs.s
#define KP_V_CAPBANK_MOD_A                      PI_CONTROLLER_V_CAPBANK_MOD_A_COEFFS.kp
#define KI_V_CAPBANK_MOD_A                      PI_CONTROLLER_V_CAPBANK_MOD_A_COEFFS.ki

#define NOTCH_FILT_2HZ_V_CAPBANK_MOD_A                  &g_controller_ctom.dsp_modules.dsp_iir_2p2z[0]
#define NOTCH_FILT_2HZ_V_CAPBANK_MOD_A_COEFFS           g_controller_ctom.dsp_modules.dsp_iir_2p2z[0].coeffs.s
#define NOTCH_FILT_4HZ_V_CAPBANK_MOD_A                  &g_controller_ctom.dsp_modules.dsp_iir_2p2z[1]
#define NOTCH_FILT_4HZ_V_CAPBANK_MOD_A_COEFFS           g_controller_ctom.dsp_modules.dsp_iir_2p2z[1].coeffs.s

#define ERROR_IOUT_RECT_MOD_A                           &g_controller_ctom.dsp_modules.dsp_error[1]

#define PI_CONTROLLER_IOUT_RECT_MOD_A                   &g_controller_ctom.dsp_modules.dsp_pi[1]
#define PI_CONTROLLER_IOUT_RECT_MOD_A_COEFFS            g_controller_mtoc.dsp_modules.dsp_pi[1].coeffs.s
#define KP_IOUT_RECT_MOD_A                              PI_CONTROLLER_IOUT_RECT_MOD_A_COEFFS.kp
#define KI_IOUT_RECT_MOD_A                              PI_CONTROLLER_IOUT_RECT_MOD_A_COEFFS.ki

#define RESSONANT_2HZ_CONTROLLER_IOUT_RECT_MOD_A            &g_controller_ctom.dsp_modules.dsp_iir_2p2z[2]
#define RESSONANT_2HZ_CONTROLLER_IOUT_RECT_MOD_A_COEFFS     g_controller_mtoc.dsp_modules.dsp_iir_2p2z[2].coeffs.s

#define RESSONANT_4HZ_CONTROLLER_IOUT_RECT_MOD_A            &g_controller_ctom.dsp_modules.dsp_iir_2p2z[3]
#define RESSONANT_4HZ_CONTROLLER_IOUT_RECT_MOD_A_COEFFS     g_controller_mtoc.dsp_modules.dsp_iir_2p2z[3].coeffs.s

#define PWM_MODULATOR_MOD_A                   g_pwm_modules.pwm_regs[0]

/**
 * Defines for AC/DC Module B
 */
#define MOD_B_ID        0x1

#define PIN_OPEN_AC_MAINS_CONTACTOR_MOD_B       CLEAR_GPDO2;
#define PIN_CLOSE_AC_MAINS_CONTACTOR_MOD_B      SET_GPDO2;
#define PIN_STATUS_AC_MAINS_CONTACTOR_MOD_B     GET_GPDI7

#define V_CAPBANK_MOD_B                 g_controller_ctom.net_signals[2].f  // HRADC2
#define IOUT_RECT_MOD_B                 g_controller_ctom.net_signals[3].f  // HRADC3

#define VOUT_RECT_MOD_B                 g_controller_mtoc.net_signals[3].f
#define TEMP_HEATSINK_MOD_B             g_controller_mtoc.net_signals[4].f
#define TEMP_INDUCTORS_MOD_B            g_controller_mtoc.net_signals[5].f

#define DUTY_CYCLE_MOD_B                g_controller_ctom.output_signals[1].f

#define ERROR_V_CAPBANK_MOD_B           &g_controller_ctom.dsp_modules.dsp_error[2]

#define PI_CONTROLLER_V_CAPBANK_MOD_B           &g_controller_ctom.dsp_modules.dsp_pi[2]
#define PI_CONTROLLER_V_CAPBANK_MOD_B_COEFFS    g_controller_mtoc.dsp_modules.dsp_pi[2].coeffs.s
#define KP_V_CAPBANK_MOD_B                      PI_CONTROLLER_V_CAPBANK_MOD_B_COEFFS.kp
#define KI_V_CAPBANK_MOD_B                      PI_CONTROLLER_V_CAPBANK_MOD_B_COEFFS.ki

#define NOTCH_FILT_2HZ_V_CAPBANK_MOD_B                  &g_controller_ctom.dsp_modules.dsp_iir_2p2z[4]
#define NOTCH_FILT_2HZ_V_CAPBANK_MOD_B_COEFFS           g_controller_ctom.dsp_modules.dsp_iir_2p2z[4].coeffs.s
#define NOTCH_FILT_4HZ_V_CAPBANK_MOD_B                  &g_controller_ctom.dsp_modules.dsp_iir_2p2z[5]
#define NOTCH_FILT_4HZ_V_CAPBANK_MOD_B_COEFFS           g_controller_ctom.dsp_modules.dsp_iir_2p2z[5].coeffs.s

#define ERROR_IOUT_RECT_MOD_B                           &g_controller_ctom.dsp_modules.dsp_error[3]

#define PI_CONTROLLER_IOUT_RECT_MOD_B                   &g_controller_ctom.dsp_modules.dsp_pi[3]
#define PI_CONTROLLER_IOUT_RECT_MOD_B_COEFFS            g_controller_mtoc.dsp_modules.dsp_pi[3].coeffs.s
#define KP_IOUT_RECT_MOD_B                              PI_CONTROLLER_IOUT_RECT_MOD_B_COEFFS.kp
#define KI_IOUT_RECT_MOD_B                              PI_CONTROLLER_IOUT_RECT_MOD_B_COEFFS.ki

#define RESSONANT_2HZ_CONTROLLER_IOUT_RECT_MOD_B            &g_controller_ctom.dsp_modules.dsp_iir_2p2z[6]
#define RESSONANT_2HZ_CONTROLLER_IOUT_RECT_MOD_B_COEFFS     g_controller_mtoc.dsp_modules.dsp_iir_2p2z[6].coeffs.s

#define RESSONANT_4HZ_CONTROLLER_IOUT_RECT_MOD_B            &g_controller_ctom.dsp_modules.dsp_iir_2p2z[7]
#define RESSONANT_4HZ_CONTROLLER_IOUT_RECT_MOD_B_COEFFS     g_controller_mtoc.dsp_modules.dsp_iir_2p2z[7].coeffs.s

#define PWM_MODULATOR_MOD_B                   g_pwm_modules.pwm_regs[1]

/**
 * Interlocks defines
 */
typedef enum
{
    CapBank_Overvoltage,
    Rectifier_Overvoltage,
    Rectifier_Undervoltage,
    Rectifier_Overcurrent,
    AC_Mains_Contactor_Fault,
    IGBT_Driver_Fault,
    DRS_Master_Interlock,
    DRS_Slave_1_Interlock,
    DRS_Slave_2_Interlock,
    DRS_Slave_3_Interlock,
    DRS_Slave_4_Interlock
} hard_interlocks_t;

typedef enum
{
    Heatsink_Overtemperature,
    Inductors_Overtemperature
} soft_interlocks_t;

#define NUM_HARD_INTERLOCKS     IGBT_Driver_Fault + 1
#define NUM_SOFT_INTERLOCKS     Inductors_Overtemperature + 1

/**
 *  Private variables
 */
static float decimation_factor;
static float decimation_coeff;


/**
 * Private functions
 */
#pragma CODE_SECTION(isr_init_controller, "ramfuncs");
#pragma CODE_SECTION(isr_controller, "ramfuncs");
#pragma CODE_SECTION(turn_off, "ramfuncs");
#pragma CODE_SECTION(process_data_udc_net_slave, "ramfuncs");
#pragma CODE_SECTION(isr_udc_net_tx_end, "ramfuncs");

static void init_peripherals_drivers(void);
static void term_peripherals_drivers(void);

static void init_controller(void);
static void init_controller_module_A(void);
static void init_controller_module_B(void);
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

static void process_data_udc_net_slave(void);
static interrupt void isr_udc_net_tx_end(void);

/**
 * Main function for this power supply module
 */
void main_fac_2p4s_acdc(void)
{
    init_controller();
    init_peripherals_drivers();
    init_interruptions();
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

    /// Initialization of HRADC boards
    stop_DMA();

    decimation_factor = HRADC_FREQ_SAMP / ISR_CONTROL_FREQ;
    decimation_coeff = 1.0 / decimation_factor;


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
    g_pwm_modules.num_modules = 2;

    PWM_MODULATOR_MOD_A = &EPwm1Regs;
    PWM_MODULATOR_MOD_B = &EPwm2Regs;

    disable_pwm_outputs();
    disable_pwm_tbclk();
    init_pwm_mep_sfo();

    /// PWM initialization
    init_pwm_module(PWM_MODULATOR_MOD_A, PWM_FREQ, 0, PWM_Sync_Master, 0,
                    PWM_ChB_Independent, PWM_DEAD_TIME);

    init_pwm_module(PWM_MODULATOR_MOD_B, PWM_FREQ, 0, PWM_Sync_Slave, 0,
                    PWM_ChB_Independent, PWM_DEAD_TIME);

    InitEPwm1Gpio();
    InitEPwm2Gpio();

    /// Initialization of timers
    InitCpuTimers();
    ConfigCpuTimer(&CpuTimer0, C28_FREQ_MHZ, 1000000);
    CpuTimer0Regs.TCR.bit.TIE = 0;

    /// Initialization of timers
    InitCpuTimers();
    ConfigCpuTimer(&CpuTimer0, C28_FREQ_MHZ, 6.5);
    CpuTimer0Regs.TCR.bit.TIE = 0;

    /// Initialization of UDC Net
    init_udc_net(1, &process_data_udc_net_slave);
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

    init_ps_module(&g_ipc_ctom.ps_module[1],
                       g_ipc_mtoc.ps_module[1].ps_status.bit.model,
                       &turn_on, &turn_off, &isr_soft_interlock,
                       &isr_hard_interlock, &reset_interlocks);

    g_ipc_ctom.ps_module[2].ps_status.all = 0;
    g_ipc_ctom.ps_module[3].ps_status.all = 0;

    init_event_manager(0, ISR_CONTROL_FREQ,
                       NUM_HARD_INTERLOCKS, NUM_SOFT_INTERLOCKS,
                       &HARD_INTERLOCKS_DEBOUNCE_TIME,
                       &HARD_INTERLOCKS_RESET_TIME,
                       &SOFT_INTERLOCKS_DEBOUNCE_TIME,
                       &SOFT_INTERLOCKS_RESET_TIME);

    init_event_manager(1, ISR_CONTROL_FREQ,
                       NUM_HARD_INTERLOCKS, NUM_SOFT_INTERLOCKS,
                       &HARD_INTERLOCKS_DEBOUNCE_TIME,
                       &HARD_INTERLOCKS_RESET_TIME,
                       &SOFT_INTERLOCKS_DEBOUNCE_TIME,
                       &SOFT_INTERLOCKS_RESET_TIME);

    init_ipc();
    init_control_framework(&g_controller_ctom);

    /*************************************/
    /** INITIALIZATION OF DSP FRAMEWORK **/
    /*************************************/

    /**
     *        name:     SRLIM_V_CAPBANK_REFERENCE
     * description:     Capacitor bank voltage reference slew-rate limiter
     *    DP class:     DSP_SRLim
     *          in:     V_CAPBANK_SETPOINT
     *         out:     V_CAPBANK_REFERENCE
     */

    init_dsp_srlim(SRLIM_V_CAPBANK_REFERENCE, MAX_REF_SLEWRATE,
                   CONTROLLER_FREQ_SAMP, &V_CAPBANK_SETPOINT,
                   &V_CAPBANK_REFERENCE);

    init_controller_module_A();
    init_controller_module_B();

    /***********************************************/
    /** INITIALIZATION OF SIGNAL GENERATOR MODULE **/
    /***********************************************/

    disable_siggen(&SIGGEN);

    init_siggen(&SIGGEN, CONTROLLER_FREQ_SAMP, &V_CAPBANK_REFERENCE);

    cfg_siggen(&SIGGEN, g_ipc_mtoc.siggen.type, g_ipc_mtoc.siggen.num_cycles,
               g_ipc_mtoc.siggen.freq, g_ipc_mtoc.siggen.amplitude,
               g_ipc_mtoc.siggen.offset, g_ipc_mtoc.siggen.aux_param);

    /**
     *        name:     SRLIM_SIGGEN_AMP
     * description:     Signal generator amplitude slew-rate limiter
     *    DP class:     DSP_SRLim
     *          in:     g_ipc_mtoc.siggen.amplitude
     *         out:     g_ipc_ctom.siggen.amplitude
     */

    init_dsp_srlim(SRLIM_SIGGEN_AMP, MAX_SR_SIGGEN_AMP,
                   CONTROLLER_FREQ_SAMP, &g_ipc_mtoc.siggen.amplitude,
                   &g_ipc_ctom.siggen.amplitude);

    /**
     *        name:     SRLIM_SIGGEN_OFFSET
     * description:     Signal generator offset slew-rate limiter
     *    DP class:     DSP_SRLim
     *          in:     g_ipc_mtoc.siggen.offset
     *         out:     g_ipc_ctom.siggen.offset
     */

    init_dsp_srlim(SRLIM_SIGGEN_OFFSET, MAX_SR_SIGGEN_OFFSET,
                   CONTROLLER_FREQ_SAMP, &g_ipc_mtoc.siggen.offset,
                   &g_ipc_ctom.siggen.offset);

    /************************************/
    /** INITIALIZATION OF TIME SLICERS **/
    /************************************/

    /**
     * Time-slicer for WfmRef sweep decimation
     */
    cfg_timeslicer(TIMESLICER_WFMREF, WFMREF_DECIMATION);

    /**
     * Time-slicer for SamplesBuffer
     */
    cfg_timeslicer(TIMESLICER_BUFFER, BUFFER_DECIMATION);

    /**
     * Time-slicer for controller
     */
    cfg_timeslicer(TIMESLICER_CONTROLLER,
                   CONTROLLER_DECIMATION);

    init_buffer(BUF_SAMPLES, &g_buf_samples_ctom, SIZE_BUF_SAMPLES_CTOM);
    enable_buffer(BUF_SAMPLES);

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
    set_pwm_duty_chA(PWM_MODULATOR_MOD_A, 0.0);
    set_pwm_duty_chA(PWM_MODULATOR_MOD_B, 0.0);

    g_ipc_ctom.ps_module[0].ps_setpoint = 0.0;
    g_ipc_ctom.ps_module[0].ps_reference = 0.0;

    reset_dsp_srlim(SRLIM_V_CAPBANK_REFERENCE);

    /// Reset capacitor bank voltage controller for module A
    reset_dsp_error(ERROR_V_CAPBANK_MOD_A);
    reset_dsp_pi(PI_CONTROLLER_V_CAPBANK_MOD_A);
    reset_dsp_iir_2p2z(NOTCH_FILT_2HZ_V_CAPBANK_MOD_A);
    reset_dsp_iir_2p2z(NOTCH_FILT_4HZ_V_CAPBANK_MOD_A);

    /// Reset rectifier output current controller for module A
    reset_dsp_error(ERROR_IOUT_RECT_MOD_A);
    reset_dsp_iir_2p2z(RESSONANT_2HZ_CONTROLLER_IOUT_RECT_MOD_A);
    reset_dsp_iir_2p2z(RESSONANT_4HZ_CONTROLLER_IOUT_RECT_MOD_A);
    reset_dsp_pi(PI_CONTROLLER_IOUT_RECT_MOD_A);

    /// Reset capacitor bank voltage controller for module B
    reset_dsp_error(ERROR_V_CAPBANK_MOD_B);
    reset_dsp_pi(PI_CONTROLLER_V_CAPBANK_MOD_B);
    reset_dsp_iir_2p2z(NOTCH_FILT_2HZ_V_CAPBANK_MOD_B);
    reset_dsp_iir_2p2z(NOTCH_FILT_4HZ_V_CAPBANK_MOD_B);

    /// Reset rectifier output current controller for module B
    reset_dsp_error(ERROR_IOUT_RECT_MOD_B);
    reset_dsp_iir_2p2z(RESSONANT_2HZ_CONTROLLER_IOUT_RECT_MOD_B);
    reset_dsp_iir_2p2z(RESSONANT_4HZ_CONTROLLER_IOUT_RECT_MOD_B);
    reset_dsp_pi(PI_CONTROLLER_IOUT_RECT_MOD_B);

    reset_dsp_srlim(SRLIM_SIGGEN_AMP);
    reset_dsp_srlim(SRLIM_SIGGEN_OFFSET);
    disable_siggen(&SIGGEN);

    reset_timeslicers();
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
    PieVectTable.EPWM1_INT = &isr_controller;
    EDIS;

    PWM_MODULATOR_MOD_A->ETSEL.bit.INTSEL = ET_CTR_ZERO;
    PWM_MODULATOR_MOD_A->ETCLR.bit.INT = 1;

    PieCtrlRegs.PIEACK.all |= M_INT3;
}

/**
 * Control ISR
 */
static interrupt void isr_controller(void)
{
    static float temp[4];
    static uint16_t i;

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

    V_CAPBANK_MOD_A = temp[0];
    IOUT_RECT_MOD_A = temp[1];
    V_CAPBANK_MOD_B = temp[2];
    IOUT_RECT_MOD_B = temp[3];

    /******** Timeslicer for controllers *********/
    RUN_TIMESLICER(TIMESLICER_CONTROLLER)
    /*********************************************/

        /// Run notch filters for capacitor bank voltage feedback
        run_dsp_iir_2p2z(NOTCH_FILT_2HZ_V_CAPBANK_MOD_A);
        run_dsp_iir_2p2z(NOTCH_FILT_4HZ_V_CAPBANK_MOD_A);

        run_dsp_iir_2p2z(NOTCH_FILT_2HZ_V_CAPBANK_MOD_B);
        run_dsp_iir_2p2z(NOTCH_FILT_4HZ_V_CAPBANK_MOD_B);

        /// Check whether power supply is ON
        if(g_ipc_ctom.ps_module[0].ps_status.bit.state > Interlock)
        {
            /// Calculate reference according to operation mode
            switch(g_ipc_ctom.ps_module[0].ps_status.bit.state)
            {
                case SlowRef:
                case SlowRefSync:
                {
                    run_dsp_srlim(SRLIM_V_CAPBANK_REFERENCE, USE_MODULE);
                    break;
                }
                case Cycle:
                {
                    /*run_dsp_srlim(SRLIM_SIGGEN_AMP, USE_MODULE);
                    run_dsp_srlim(SRLIM_SIGGEN_OFFSET, USE_MODULE);
                    SIGGEN.p_run_siggen(&SIGGEN);
                    break;*/
                }
                case RmpWfm:
                {
                    break;
                }
                case MigWfm:
                {
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
                SATURATE(V_CAPBANK_REFERENCE, MAX_REF_OL, MIN_REF_OL);
                DUTY_CYCLE_MOD_A = 0.01 * V_CAPBANK_REFERENCE;
                SATURATE(DUTY_CYCLE_MOD_A, PWM_MAX_DUTY_OL, PWM_MIN_DUTY_OL);
                DUTY_CYCLE_MOD_B = DUTY_CYCLE_MOD_A;
            }
            /// Closed-loop
            else
            {
                /// Run capacitor bank voltage control law
                SATURATE(g_ipc_ctom.ps_module[0].ps_reference, MAX_REF, MIN_REF);

                run_dsp_error(ERROR_V_CAPBANK_MOD_A);
                run_dsp_pi(PI_CONTROLLER_V_CAPBANK_MOD_A);

                run_dsp_error(ERROR_V_CAPBANK_MOD_B);
                run_dsp_pi(PI_CONTROLLER_V_CAPBANK_MOD_B);


                /// Run rectifier output current control law
                run_dsp_error(ERROR_IOUT_RECT_MOD_A);
                run_dsp_iir_2p2z(RESSONANT_2HZ_CONTROLLER_IOUT_RECT_MOD_A);
                run_dsp_iir_2p2z(RESSONANT_4HZ_CONTROLLER_IOUT_RECT_MOD_A);
                run_dsp_pi(PI_CONTROLLER_IOUT_RECT_MOD_A);
                SATURATE(DUTY_CYCLE_MOD_A, PWM_MAX_DUTY, PWM_MIN_DUTY);

                run_dsp_error(ERROR_IOUT_RECT_MOD_B);
                run_dsp_iir_2p2z(RESSONANT_2HZ_CONTROLLER_IOUT_RECT_MOD_B);
                run_dsp_iir_2p2z(RESSONANT_4HZ_CONTROLLER_IOUT_RECT_MOD_B);
                run_dsp_pi(PI_CONTROLLER_IOUT_RECT_MOD_B);
                SATURATE(DUTY_CYCLE_MOD_B, PWM_MAX_DUTY, PWM_MIN_DUTY);
            }

            set_pwm_duty_chA(PWM_MODULATOR_MOD_A, DUTY_CYCLE_MOD_A);
            set_pwm_duty_chA(PWM_MODULATOR_MOD_B, DUTY_CYCLE_MOD_B);
        }

    /*********************************************/
    END_TIMESLICER(TIMESLICER_CONTROLLER)
    /*********************************************/

    /******** Timeslicer for samples buffer ******/
    RUN_TIMESLICER(TIMESLICER_BUFFER)
    /*********************************************/
        insert_buffer(BUF_SAMPLES, NETSIGNAL_CTOM_BUF);
    /*********************************************/
    END_TIMESLICER(TIMESLICER_BUFFER)
    /*********************************************/

    SET_INTERLOCKS_TIMEBASE_FLAG(0);
    SET_INTERLOCKS_TIMEBASE_FLAG(1);

    PWM_MODULATOR_MOD_A->ETCLR.bit.INT = 1;
    PieCtrlRegs.PIEACK.all |= M_INT3;

    CLEAR_DEBUG_GPIO1;
}

/**
 * Initialization of interruptions.
 */
static void init_interruptions(void)
{
    EALLOW;
    PieVectTable.EPWM1_INT =  &isr_init_controller;
    PieVectTable.TINT0 =      &isr_udc_net_tx_end;
    EDIS;

    PieCtrlRegs.PIEIER1.bit.INTx7 = 1;

    PieCtrlRegs.PIEIER3.bit.INTx1 = 1;
    enable_pwm_interrupt(PWM_MODULATOR_MOD_A);

    PieCtrlRegs.PIEIER9.bit.INTx1=1;

    /**
     * Enable interrupt groups related to:
     *  INT1:  External sync
     *  INT3:  PWM
     *  INT9:  SCI RX FIFO
     *  INT11: IPC MTOC
     */
    IER |= M_INT1;
    IER |= M_INT3;
    IER |= M_INT9;
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
    PieCtrlRegs.PIEIER3.bit.INTx1 = 0;  /// ePWM1
    PieCtrlRegs.PIEIER9.bit.INTx1 = 0;  /// SCI RX
    disable_pwm_interrupt(PWM_MODULATOR_MOD_A);

    /// Clear flags
    PieCtrlRegs.PIEACK.all |= M_INT1 | M_INT3 | M_INT9 | M_INT11;
}

/**
 * Turn power supply on.
 *
 * @param dummy dummy argument due to ps_module pointer
 */
static void turn_on(uint16_t dummy)
{
    #ifdef USE_ITLK
    if(g_ipc_ctom.ps_module[MOD_A_ID].ps_status.bit.state == Off)
    #else
    if(g_ipc_ctom.ps_module[MOD_A_ID].ps_status.bit.state <= Interlock)
    #endif
    {
        reset_controller();

        g_ipc_ctom.ps_module[MOD_A_ID].ps_status.bit.state = Initializing;

        PIN_CLOSE_AC_MAINS_CONTACTOR_MOD_A;
        PIN_CLOSE_AC_MAINS_CONTACTOR_MOD_B;

        DELAY_US(TIMEOUT_AC_MAINS_CONTACTOR_CLOSED_MS*1000);

        if(!PIN_STATUS_AC_MAINS_CONTACTOR_MOD_A)
        {
            BYPASS_HARD_INTERLOCK_DEBOUNCE(MOD_A_ID, AC_Mains_Contactor_Fault);
            set_hard_interlock(MOD_A_ID, AC_Mains_Contactor_Fault);
        }

        if(!PIN_STATUS_AC_MAINS_CONTACTOR_MOD_B)
        {
            BYPASS_HARD_INTERLOCK_DEBOUNCE(MOD_B_ID, AC_Mains_Contactor_Fault);
            set_hard_interlock(MOD_B_ID, AC_Mains_Contactor_Fault);
            #ifdef USE_ITLK
            g_ipc_ctom.ps_module[MOD_A_ID].ps_status.bit.state = Interlock;
            #endif
        }

        if(g_ipc_ctom.ps_module[MOD_A_ID].ps_status.bit.state == Initializing)
        {
            g_ipc_ctom.ps_module[MOD_A_ID].ps_status.bit.openloop = OPEN_LOOP;
            g_ipc_ctom.ps_module[MOD_A_ID].ps_status.bit.state = SlowRef;
            enable_pwm_output(MOD_A_ID);
            enable_pwm_output(MOD_B_ID);
        }
    }
}

/**
 * Turn off specified power supply.
 *
 * @param dummy dummy argument due to ps_module pointer
 */
static void turn_off(uint16_t dummy)
{
    disable_pwm_output(MOD_A_ID);
    disable_pwm_output(MOD_B_ID);

    PIN_OPEN_AC_MAINS_CONTACTOR_MOD_A;
    PIN_OPEN_AC_MAINS_CONTACTOR_MOD_B;

    DELAY_US(TIMEOUT_AC_MAINS_CONTACTOR_OPENED_MS*1000);

    reset_controller();

    if(g_ipc_ctom.ps_module[MOD_A_ID].ps_status.bit.state != Interlock)
    {
        g_ipc_ctom.ps_module[MOD_A_ID].ps_status.bit.state = Off;
        g_ipc_ctom.ps_module[MOD_B_ID].ps_status.bit.state = Off;
    }
}

/**
 * Reset interlocks for specified power supply. Variable state from ps_module[0]
 * is shared between both modules.
 *
 * @param dummy dummy argument due to ps_module pointer
 */
static void reset_interlocks(uint16_t dummy)
{
    g_ipc_ctom.ps_module[MOD_A_ID].ps_hard_interlock = 0;
    g_ipc_ctom.ps_module[MOD_A_ID].ps_soft_interlock = 0;

    g_ipc_ctom.ps_module[MOD_B_ID].ps_hard_interlock = 0;
    g_ipc_ctom.ps_module[MOD_B_ID].ps_soft_interlock = 0;

    if(g_ipc_ctom.ps_module[MOD_A_ID].ps_status.bit.state < Initializing)
    {
        g_ipc_ctom.ps_module[MOD_A_ID].ps_status.bit.state = Off;
        g_ipc_ctom.ps_module[MOD_B_ID].ps_status.bit.state = Off;
    }
}

/**
 * Check interlocks of this specific power supply topology
 */
static inline void check_interlocks(void)
{
    if(fabs(V_CAPBANK_MOD_A) > MAX_V_CAPBANK)
    {
        set_hard_interlock(MOD_A_ID, CapBank_Overvoltage);
    }

    if(fabs(V_CAPBANK_MOD_B) > MAX_V_CAPBANK)
    {
        set_hard_interlock(MOD_B_ID, CapBank_Overvoltage);
    }

    if(fabs(IOUT_RECT_MOD_A) > MAX_IOUT_RECT)
    {
        set_hard_interlock(MOD_A_ID, Rectifier_Overcurrent);
    }

    if(fabs(IOUT_RECT_MOD_B) > MAX_IOUT_RECT)
    {
        set_hard_interlock(MOD_B_ID, Rectifier_Overcurrent);
    }

    DINT;

    if ( (g_ipc_ctom.ps_module[0].ps_status.bit.state <= Interlock) &&
         (PIN_STATUS_AC_MAINS_CONTACTOR_MOD_A) )
    {
        set_hard_interlock(MOD_A_ID, AC_Mains_Contactor_Fault);
    }

    else if ( (g_ipc_ctom.ps_module[0].ps_status.bit.state > Interlock)
              && (!PIN_STATUS_AC_MAINS_CONTACTOR_MOD_A) )
    {
        set_hard_interlock(MOD_A_ID, AC_Mains_Contactor_Fault);
    }

    if ( (g_ipc_ctom.ps_module[0].ps_status.bit.state <= Interlock) &&
         (PIN_STATUS_AC_MAINS_CONTACTOR_MOD_B) )
    {
        set_hard_interlock(MOD_B_ID, AC_Mains_Contactor_Fault);
    }

    else if ( (g_ipc_ctom.ps_module[0].ps_status.bit.state > Interlock)
              && (!PIN_STATUS_AC_MAINS_CONTACTOR_MOD_B) )
    {
        set_hard_interlock(MOD_B_ID, AC_Mains_Contactor_Fault);
    }
    EINT;

    if(g_ipc_ctom.ps_module[MOD_B_ID].ps_status.bit.state == Interlock)
    {
        g_ipc_ctom.ps_module[MOD_A_ID].ps_status.bit.state = Interlock;
    }

    //SET_DEBUG_GPIO1;
    run_interlocks_debouncing(0);
    run_interlocks_debouncing(1);
    //CLEAR_DEBUG_GPIO1;
}

static void init_controller_module_A(void)
{
    /************************************************************************/
    /** INITIALIZATION OF CAPACITOR BANK VOLTAGE CONTROL LOOP FOR MODULE A **/
    /************************************************************************/

    /**
     *        name:     ERROR_V_CAPBANK_MOD_A
     * description:     Capacitor bank voltage reference error for module A
     *  dsp module:     DSP_Error
     *           +:     ps_module[0].ps_reference
     *           -:     net_signals[5]
     *         out:     net_signals[6]
     */

    init_dsp_error(ERROR_V_CAPBANK_MOD_A, &V_CAPBANK_REFERENCE,
                   &g_controller_ctom.net_signals[5].f,
                   &g_controller_ctom.net_signals[6].f);

    /**
     *        name:     PI_CONTROLLER_V_CAPBANK_MOD_A
     * description:     Capacitor bank voltage PI controller for module A
     *  dsp module:     DSP_PI
     *          in:     net_signals[6]
     *         out:     net_signals[7]
     */

    init_dsp_pi(PI_CONTROLLER_V_CAPBANK_MOD_A, KP_V_CAPBANK_MOD_A,
                KI_V_CAPBANK_MOD_A, CONTROLLER_FREQ_SAMP, MAX_IOUT_RECT_REF,
                MIN_IOUT_RECT_REF, &g_controller_ctom.net_signals[6].f,
                &g_controller_ctom.net_signals[7].f);

    /**
     *        name:     NOTCH_FILT_2HZ_V_CAPBANK_MOD_A
     * description:     Cap bank voltage notch filter (fcut = 2 Hz) for module A
     *    DP class:     DSP_IIR_2P2Z
     *          in:     net_signals[0]
     *         out:     net_signals[4]
     */

    init_dsp_notch_2p2z(NOTCH_FILT_2HZ_V_CAPBANK_MOD_A, NF_ALPHA, 2.0,
                        CONTROLLER_FREQ_SAMP, FLT_MAX, -FLT_MAX,
                        &V_CAPBANK_MOD_A, &g_controller_ctom.net_signals[4].f);

    /*init_dsp_iir_2p2z(NOTCH_FILT_2HZ_V_CAPBANK_MOD_A,
                      NOTCH_FILT_2HZ_V_CAPBANK_MOD_A_COEFFS.b0,
                      NOTCH_FILT_2HZ_V_CAPBANK_MOD_A_COEFFS.b1,
                      NOTCH_FILT_2HZ_V_CAPBANK_MOD_A_COEFFS.b2,
                      NOTCH_FILT_2HZ_V_CAPBANK_MOD_A_COEFFS.a1,
                      NOTCH_FILT_2HZ_V_CAPBANK_MOD_A_COEFFS.a2,
                      FLT_MAX, -FLT_MAX,
                      &V_CAPBANK_MOD_A, &g_controller_ctom.net_signals[4].f);*/

    /**
     *        name:     NOTCH_FILT_4HZ_V_CAPBANK_MOD_A
     * description:     Cap bank voltage notch filter (fcut = 4 Hz) for module A
     *    DP class:     DSP_IIR_2P2Z
     *          in:     net_signals[4]
     *         out:     net_signals[5]
     */

    init_dsp_notch_2p2z(NOTCH_FILT_4HZ_V_CAPBANK_MOD_A, NF_ALPHA, 4.0,
                        CONTROLLER_FREQ_SAMP, FLT_MAX, -FLT_MAX,
                        &g_controller_ctom.net_signals[4].f,
                        &g_controller_ctom.net_signals[5].f);

    /*init_dsp_iir_2p2z(NOTCH_FILT_4HZ_V_CAPBANK_MOD_A,
                      NOTCH_FILT_4HZ_V_CAPBANK_MOD_A_COEFFS.b0,
                      NOTCH_FILT_4HZ_V_CAPBANK_MOD_A_COEFFS.b1,
                      NOTCH_FILT_4HZ_V_CAPBANK_MOD_A_COEFFS.b2,
                      NOTCH_FILT_4HZ_V_CAPBANK_MOD_A_COEFFS.a1,
                      NOTCH_FILT_4HZ_V_CAPBANK_MOD_A_COEFFS.a2,
                      FLT_MAX, -FLT_MAX,
                      &g_controller_ctom.net_signals[4].f,
                      &g_controller_ctom.net_signals[5].f);*/

    /**************************************************************************/
    /** INITIALIZATION OF RECTIFIER OUTPUT CURRENT CONTROL LOOP FOR MODULE A **/
    /**************************************************************************/

    /**
     *        name:     ERROR_IOUT_RECT_MOD_A
     * description:     Rectifier output current reference error for module A
     *    DP class:     DSP_Error
     *           +:     net_signals[7]
     *           -:     net_signals[1]
     *         out:     net_signals[8]
     */

    init_dsp_error(ERROR_IOUT_RECT_MOD_A, &g_controller_ctom.net_signals[7].f,
                   &IOUT_RECT_MOD_A, &g_controller_ctom.net_signals[8].f);

    /**
     *        name:     RESSONANT_2HZ_CONTROLLER_IOUT_RECT_MOD_A
     * description:     Rectifier output current 2 Hz ressonant controller for module A
     *    DP class:     ELP_IIR_2P2Z
     *          in:     net_signals[8]
     *         out:     net_signals[9]
     */

    init_dsp_iir_2p2z(RESSONANT_2HZ_CONTROLLER_IOUT_RECT_MOD_A,
                      RESSONANT_2HZ_CONTROLLER_IOUT_RECT_MOD_A_COEFFS.b0,
                      RESSONANT_2HZ_CONTROLLER_IOUT_RECT_MOD_A_COEFFS.b1,
                      RESSONANT_2HZ_CONTROLLER_IOUT_RECT_MOD_A_COEFFS.b2,
                      RESSONANT_2HZ_CONTROLLER_IOUT_RECT_MOD_A_COEFFS.a1,
                      RESSONANT_2HZ_CONTROLLER_IOUT_RECT_MOD_A_COEFFS.a2,
                      FLT_MAX, -FLT_MAX,
                      &g_controller_ctom.net_signals[8].f,
                      &g_controller_ctom.net_signals[9].f);

    /**
     *        name:     RESSONANT_4HZ_CONTROLLER_IOUT_RECT_MOD_A
     * description:     Rectifier output current 4 Hz ressonant controller for module A
     *    DP class:     ELP_IIR_2P2Z
     *          in:     net_signals[9]
     *         out:     net_signals[10]
     */

    init_dsp_iir_2p2z(RESSONANT_4HZ_CONTROLLER_IOUT_RECT_MOD_A,
                      RESSONANT_4HZ_CONTROLLER_IOUT_RECT_MOD_A_COEFFS.b0,
                      RESSONANT_4HZ_CONTROLLER_IOUT_RECT_MOD_A_COEFFS.b1,
                      RESSONANT_4HZ_CONTROLLER_IOUT_RECT_MOD_A_COEFFS.b2,
                      RESSONANT_4HZ_CONTROLLER_IOUT_RECT_MOD_A_COEFFS.a1,
                      RESSONANT_4HZ_CONTROLLER_IOUT_RECT_MOD_A_COEFFS.a2,
                      FLT_MAX, -FLT_MAX,
                      &g_controller_ctom.net_signals[9].f,
                      &g_controller_ctom.net_signals[10].f);

    /**
     *        name:     PI_CONTROLLER_IOUT_RECT_MOD_A
     * description:     Rectifier output current PI controller for module A
     *    DP class:     DSP_PI
     *          in:     net_signals[10]
     *         out:     output_signals[0]
     */
    init_dsp_pi(PI_CONTROLLER_IOUT_RECT_MOD_A, KP_IOUT_RECT_MOD_A,
                KI_IOUT_RECT_MOD_A, CONTROLLER_FREQ_SAMP, PWM_MAX_DUTY,
                PWM_MIN_DUTY, &g_controller_ctom.net_signals[10].f,
                &DUTY_CYCLE_MOD_A);
}

static void init_controller_module_B(void)
{
    /************************************************************************/
    /** INITIALIZATION OF CAPACITOR BANK VOLTAGE CONTROL LOOP FOR MODULE B **/
    /************************************************************************/

    /**
     *        name:     ERROR_V_CAPBANK_MOD_B
     * description:     Capacitor bank voltage reference error for module B
     *  dsp module:     DSP_Error
     *           +:     ps_module[0].ps_reference
     *           -:     net_signals[12]
     *         out:     net_signals[13]
     */

    init_dsp_error(ERROR_V_CAPBANK_MOD_B, &V_CAPBANK_REFERENCE,
                   &g_controller_ctom.net_signals[12].f,
                   &g_controller_ctom.net_signals[13].f);

    /**
     *        name:     PI_CONTROLLER_V_CAPBANK_MOD_B
     * description:     Capacitor bank voltage PI controller for module B
     *  dsp module:     DSP_PI
     *          in:     net_signals[13]
     *         out:     net_signals[14]
     */

    init_dsp_pi(PI_CONTROLLER_V_CAPBANK_MOD_B, KP_V_CAPBANK_MOD_B,
                KI_V_CAPBANK_MOD_B, CONTROLLER_FREQ_SAMP, MAX_IOUT_RECT_REF,
                MIN_IOUT_RECT_REF, &g_controller_ctom.net_signals[13].f,
                &g_controller_ctom.net_signals[14].f);

    /**
     *        name:     NOTCH_FILT_2HZ_V_CAPBANK_MOD_B
     * description:     Cap bank voltage notch filter (fcut = 2 Hz) for module B
     *    DP class:     DSP_IIR_2P2Z
     *          in:     net_signals[2]
     *         out:     net_signals[11]
     */

    init_dsp_notch_2p2z(NOTCH_FILT_2HZ_V_CAPBANK_MOD_B, NF_ALPHA, 2.0,
                        CONTROLLER_FREQ_SAMP, FLT_MAX, -FLT_MAX,
                        &V_CAPBANK_MOD_B, &g_controller_ctom.net_signals[11].f);

    /*init_dsp_iir_2p2z(NOTCH_FILT_2HZ_V_CAPBANK_MOD_B,
                      NOTCH_FILT_2HZ_V_CAPBANK_MOD_B_COEFFS.b0,
                      NOTCH_FILT_2HZ_V_CAPBANK_MOD_B_COEFFS.b1,
                      NOTCH_FILT_2HZ_V_CAPBANK_MOD_B_COEFFS.b2,
                      NOTCH_FILT_2HZ_V_CAPBANK_MOD_B_COEFFS.a1,
                      NOTCH_FILT_2HZ_V_CAPBANK_MOD_B_COEFFS.a2,
                      FLT_MAX, -FLT_MAX,
                      &V_CAPBANK_MOD_B, &g_controller_ctom.net_signals[11].f);*/

    /**
     *        name:     NOTCH_FILT_4HZ_V_CAPBANK_MOD_B
     * description:     Cap bank voltage notch filter (fcut = 4 Hz) for module B
     *    DP class:     DSP_IIR_2P2Z
     *          in:     net_signals[11]
     *         out:     net_signals[12]
     */

    init_dsp_notch_2p2z(NOTCH_FILT_4HZ_V_CAPBANK_MOD_B, NF_ALPHA, 4.0,
                        CONTROLLER_FREQ_SAMP, FLT_MAX, -FLT_MAX,
                        &g_controller_ctom.net_signals[11].f,
                        &g_controller_ctom.net_signals[12].f);

    /*init_dsp_iir_2p2z(NOTCH_FILT_4HZ_V_CAPBANK_MOD_B,
                      NOTCH_FILT_4HZ_V_CAPBANK_MOD_B_COEFFS.b0,
                      NOTCH_FILT_4HZ_V_CAPBANK_MOD_B_COEFFS.b1,
                      NOTCH_FILT_4HZ_V_CAPBANK_MOD_B_COEFFS.b2,
                      NOTCH_FILT_4HZ_V_CAPBANK_MOD_B_COEFFS.a1,
                      NOTCH_FILT_4HZ_V_CAPBANK_MOD_B_COEFFS.a2,
                      FLT_MAX, -FLT_MAX,
                      &g_controller_ctom.net_signals[11].f,
                      &g_controller_ctom.net_signals[12].f);*/

    /**************************************************************************/
    /** INITIALIZATION OF RECTIFIER OUTPUT CURRENT CONTROL LOOP FOR MODULE B **/
    /**************************************************************************/

    /**
     *        name:     ERROR_IOUT_RECT_MOD_B
     * description:     Rectifier output current reference error for module B
     *    DP class:     DSP_Error
     *           +:     net_signals[14]
     *           -:     net_signals[3]
     *         out:     net_signals[15]
     */

    init_dsp_error(ERROR_IOUT_RECT_MOD_B, &g_controller_ctom.net_signals[14].f,
                   &IOUT_RECT_MOD_B, &g_controller_ctom.net_signals[15].f);

    /**
     *        name:     RESSONANT_2HZ_CONTROLLER_IOUT_RECT_MOD_B
     * description:     Rectifier output current 2 Hz ressonant controller for module B
     *    DP class:     ELP_IIR_2P2Z
     *          in:     net_signals[15]
     *         out:     net_signals[16]
     */

    init_dsp_iir_2p2z(RESSONANT_2HZ_CONTROLLER_IOUT_RECT_MOD_B,
                      RESSONANT_2HZ_CONTROLLER_IOUT_RECT_MOD_B_COEFFS.b0,
                      RESSONANT_2HZ_CONTROLLER_IOUT_RECT_MOD_B_COEFFS.b1,
                      RESSONANT_2HZ_CONTROLLER_IOUT_RECT_MOD_B_COEFFS.b2,
                      RESSONANT_2HZ_CONTROLLER_IOUT_RECT_MOD_B_COEFFS.a1,
                      RESSONANT_2HZ_CONTROLLER_IOUT_RECT_MOD_B_COEFFS.a2,
                      FLT_MAX, -FLT_MAX,
                      &g_controller_ctom.net_signals[15].f,
                      &g_controller_ctom.net_signals[16].f);

    /**
     *        name:     RESSONANT_4HZ_CONTROLLER_IOUT_RECT_MOD_B
     * description:     Rectifier output current 4 Hz ressonant controller for module B
     *    DP class:     ELP_IIR_2P2Z
     *          in:     net_signals[16]
     *         out:     net_signals[17]
     */

    init_dsp_iir_2p2z(RESSONANT_4HZ_CONTROLLER_IOUT_RECT_MOD_B,
                      RESSONANT_4HZ_CONTROLLER_IOUT_RECT_MOD_B_COEFFS.b0,
                      RESSONANT_4HZ_CONTROLLER_IOUT_RECT_MOD_B_COEFFS.b1,
                      RESSONANT_4HZ_CONTROLLER_IOUT_RECT_MOD_B_COEFFS.b2,
                      RESSONANT_4HZ_CONTROLLER_IOUT_RECT_MOD_B_COEFFS.a1,
                      RESSONANT_4HZ_CONTROLLER_IOUT_RECT_MOD_B_COEFFS.a2,
                      FLT_MAX, -FLT_MAX,
                      &g_controller_ctom.net_signals[16].f,
                      &g_controller_ctom.net_signals[17].f);

    /**
     *        name:     PI_CONTROLLER_IOUT_RECT_MOD_B
     * description:     Rectifier output current PI controller for module B
     *    DP class:     DSP_PI
     *          in:     net_signals[17]
     *         out:     output_signals[1]
     */
    init_dsp_pi(PI_CONTROLLER_IOUT_RECT_MOD_B, KP_IOUT_RECT_MOD_B,
                KI_IOUT_RECT_MOD_B, CONTROLLER_FREQ_SAMP, PWM_MAX_DUTY,
                PWM_MIN_DUTY, &g_controller_ctom.net_signals[17].f,
                &DUTY_CYCLE_MOD_B);
}

/**
 *
 */
static void process_data_udc_net_slave(void)
{
    switch(g_udc_net.recv_msg.bit.cmd)
    {
        case Turn_On_UDC_Net:
        {
            turn_on(0);
            break;
        }

        case Turn_Off_UDC_Net:
        {
            turn_off(0);
            break;
        }

        case Set_Interlock_UDC_Net:
        {
            set_hard_interlock(0, DRS_Master_Interlock +
                                  g_udc_net.recv_msg.bit.data);
            break;
        }

        case Reset_Interlock_UDC_Net:
        {
            reset_interlocks(0);
            break;
        }

        case Get_Status_UDC_Net:
        {
            if(g_ipc_ctom.ps_module[0].ps_status.bit.state == Interlock)
            {
                set_interlock_udc_net();
            }
            else
            {
            send_udc_net_cmd( 0, Get_Status_UDC_Net,
                              (uint16_t) g_ipc_ctom.ps_module[0].ps_status.all );
            }

            break;
        }

        default:
        {
            break;
        }
    }
    CpuTimer0Regs.TCR.all = 0x4020;
}

static interrupt void isr_udc_net_tx_end(void)
{
    RESET_SCI_RD;
    CpuTimer0Regs.TCR.all = 0xC010;
    CLEAR_DEBUG_GPIO1;
    PieCtrlRegs.PIEACK.all |= PIEACK_GROUP1;
}
