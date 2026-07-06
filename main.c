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
 * @file main.c
 * @brief Main file of firmware for C28 core from DRS-UDC board.
 *
 * Main file of firmware for C28 core from DRS-UDC board. This firmware
 * implements digital controllers for magnet power supplies from Sirius Project.
 * 
 * At initialization, the ARM core reads from non-volatile memory which power
 * supply model the controller is set, in order to both cores be initialized
 * with the proper power supply module (ps_module).
 *
 * @author gabriel.brunheira
 * @date 20/10/2017
 *
 */

#include <string.h>
#include "boards/udc_c28.h"
#include "ipc/ipc.h"
#include "parameters/parameters.h"

#include "ps_modules/fbp.h"
#include "ps_modules/fbp_dclink.h"
#include "ps_modules/fac_acdc.h"
#include "ps_modules/fac_dcdc.h"
#include "ps_modules/fac_2s_acdc.h"
#include "ps_modules/fac_2s_dcdc.h"
#include "ps_modules/fac_2p4s_acdc.h"
#include "ps_modules/fac_2p4s_dcdc.h"
#include "ps_modules/fac_2p_acdc_imas.h"
#include "ps_modules/fac_2p_dcdc_imas.h"
#include "ps_modules/fap.h"
#include "ps_modules/fap_4p.h"
#include "ps_modules/fac_dcdc_ema.h"
#include "ps_modules/fap_2p2s.h"
#include "ps_modules/resonant_swls.h"
#include "ps_modules/uninitialized.h"

/**
 * @brief Main function
 */
void main(void)
{
    /**
     * Initialize the Control System:
     * Enable peripheral clocks
     * This example function is found in the F28M36x_SysCtrl.c file.
     */
    InitSysCtrl();

    /**
     * Copy time critical code and Flash setup code to RAM
     * This includes the following functions:  InitFlash();
     * The  RamfuncsLoadStart, RamfuncsLoadSize, and RamfuncsRunStart
     * symbols are created by the linker. Refer to the device .cmd file.
     */
    memcpy(&RamfuncsRunStart, &RamfuncsLoadStart, (size_t)&RamfuncsLoadSize);

    /**
     * Call Flash Initialization to setup flash waitstates
     * This function must reside in RAM
     */
    InitFlash();

    /**
     * Disable CPU interrupts
     */
    DINT;

    /**
     * Initialize the PIE control registers to their default state.
     * The default state is all PIE interrupts disabled and flags are cleared.
     * This function is found in the F28M36x_PieCtrl.c file.
     */
    InitPieCtrl();

    /**
     * Disable CPU interrupts and clear all CPU interrupt flags:
     */
    IER = 0x0000;
    IFR = 0x0000;

    /**
     * Initialize the PIE vector table with pointers to the shell Interrupt
     * Service Routines (ISR).
     * This will populate the entire table, even if the interrupt
     * is not used in this example.  This is useful for debug purposes.
     * The shell ISR routines are found in F28M36x_DefaultIsr.c.
     * This function is found in F28M36x_PieVect.c.
     */
    InitPieVectTable();

    /**
     *  TODO: Make sure ARM is already initialized to continue from here
     */
    init_gpios();
    init_buzzer(BUZZER_VOLUME);

    while(1)
    {
        /**
         * Select power supply module
         */
        switch(PS_MODEL)
        {
            case FBP:
            {
                main_fbp();
                break;
            }

            case FBP_DCLink:
            {
                main_fbp_dclink();
                break;
            }

            case FAC_ACDC:
            {
                main_fac_acdc();
                break;
            }

            case FAC_DCDC:
            {
                main_fac_dcdc();
                break;
            }

            case FAC_2S_ACDC:
            {
                main_fac_2s_acdc();
                break;
            }

            case FAC_2S_DCDC:
            {
                main_fac_2s_dcdc();
                break;
            }

            case FAC_2P4S_ACDC:
            {
                main_fac_2p4s_acdc();
                break;
            }

            case FAC_2P4S_DCDC:
            {
                main_fac_2p4s_dcdc();
                break;
            }

            case FAP:
            {
                main_fap();
                break;
            }

            case FAP_4P:
            {
                main_fap_4p();
                break;
            }

            case FAC_DCDC_EMA:
            {
                //main_fac_dcdc_ema();
                break;
            }

            case FAP_2P2S:
            {
                main_fap_2p2s();
                break;
            }

            case FAP_IMAS:
            {
                //main_fap_imas();
                main_uninitialized();
                break;
            }

            case FAC_2P_ACDC_IMAS:
            {
                //main_fac_2p_acdc_imas();
                main_uninitialized();
                break;
            }

            case FAC_2P_DCDC_IMAS:
            {
                //main_fac_2p_dcdc_imas();
                main_uninitialized();
                break;
            }

            case Uninitialized:
            {
                main_uninitialized();
                break;
            }

            case RESONANT_SWLS:
            {
                main_resonant_swls();
                break;
            }

            default:
            {
                break;
            }
        }

    }
}

/**
 * TODO: Put here the implementation for your private functions.
 */
