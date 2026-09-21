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
 * @file udc_c28.h
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

#ifndef UDC_C28_H_
#define UDC_C28_H_

#include <stdint.h>

#include "DSP28x_Project.h"

/**
 * Select DRS boards version
 */
#define UDC_V2_0    0
#define UDC_V2_1    1

#define HRADC_v2_0  0
#define HRADC_v2_1  1

/**
 * C28 system defines
 */
#define C28_FREQ_MHZ    150
#define LSPCLK_DV       1       /// Divide LSPCLOCK by 2 (used by McBSP and SCI)

/**
 * GPIO macros
 */

#define SET_DEBUG_GPIO0         GpioDataRegs.GPBSET.bit.GPIO46 = 1;
#define CLEAR_DEBUG_GPIO0       GpioDataRegs.GPBCLEAR.bit.GPIO46 = 1;

#define SET_DEBUG_GPIO1         GpioDataRegs.GPDSET.bit.GPIO111 = 1;
#define CLEAR_DEBUG_GPIO1       GpioDataRegs.GPDCLEAR.bit.GPIO111 = 1;

#define SET_GPDO1               GpioDataRegs.GPCSET.bit.GPIO67 = 1;
#define CLEAR_GPDO1             GpioDataRegs.GPCCLEAR.bit.GPIO67 = 1;

#define SET_GPDO2               GpioDataRegs.GPCSET.bit.GPIO65 = 1;
#define CLEAR_GPDO2             GpioDataRegs.GPCCLEAR.bit.GPIO65 = 1;

#define SET_GPDO3               GpioDataRegs.GPCSET.bit.GPIO66 = 1;
#define CLEAR_GPDO3             GpioDataRegs.GPCCLEAR.bit.GPIO66 = 1;

#define SET_GPDO4               GpioDataRegs.GPCSET.bit.GPIO64 = 1;
#define CLEAR_GPDO4             GpioDataRegs.GPCCLEAR.bit.GPIO64 = 1;

#define SET_ALL_GPDO            GpioDataRegs.GPCSET.all = 0x0000000F;
#define CLEAR_ALL_GPDO          GpioDataRegs.GPCCLEAR.all = 0x0000000F;

#define SET_EPWMSYNCO           GpioDataRegs.GPBCLEAR.bit.GPIO33 = 1;
#define CLEAR_EPWMSYNCO         GpioDataRegs.GPBSET.bit.GPIO33 = 1;

#define GET_GPDI1               GpioDataRegs.GPDDAT.bit.GPIO126
#define GET_GPDI2               GpioDataRegs.GPDDAT.bit.GPIO127
#define GET_GPDI3               GpioDataRegs.GPDDAT.bit.GPIO124
#define GET_GPDI4               GpioDataRegs.GPDDAT.bit.GPIO125
#define GET_GPDI5               GpioG2DataRegs.GPGDAT.bit.GPIO195
#define GET_GPDI6               GpioDataRegs.GPDDAT.bit.GPIO116
#define GET_GPDI7               GpioG2DataRegs.GPGDAT.bit.GPIO194
#define GET_GPDI8               GpioG2DataRegs.GPGDAT.bit.GPIO192

#if UDC_V2_1
#define GET_GPDI9               GpioDataRegs.GPDDAT.bit.GPIO109
#define GET_GPDI10              GpioDataRegs.GPDDAT.bit.GPIO110
#define GET_EPWMSYNCI           GpioDataRegs.GPBDAT.bit.GPIO38
#elif UDC_V2_0
#define GET_GPDI9               GpioDataRegs.GPDDAT.bit.GPIO115
#define GET_GPDI10              GpioDataRegs.GPDDAT.bit.GPIO114
#define GET_EPWMSYNCI           GpioDataRegs.GPBDAT.bit.GPIO32
#endif

#define GET_GPDI11              GpioDataRegs.GPDDAT.bit.GPIO113
#define GET_GPDI12              GpioDataRegs.GPDDAT.bit.GPIO112
#define GET_GPDI13              GpioG2DataRegs.GPGDAT.bit.GPIO197
#define GET_GPDI14              GpioG2DataRegs.GPGDAT.bit.GPIO196
#define GET_GPDI15              GpioG2DataRegs.GPGDAT.bit.GPIO198
#define GET_GPDI16              GpioG2DataRegs.GPGDAT.bit.GPIO199

#define GET_INT_ARM             (GpioDataRegs.GPADAT.bit.GPIO28)
#define GET_INT_C28             (GpioDataRegs.GPADAT.bit.GPIO29)
#define GET_INT_GENERAL         (!GpioDataRegs.GPBDAT.bit.GPIO55)
#define GET_SYNC_IN             (!GpioDataRegs.GPBDAT.bit.GPIO55)

#define GET_EPWMSYNCO           (!GpioDataRegs.GPBDAT.bit.GPIO33)


/**
 * Initialization of all digital inputs and outputs, including two debug GPIOs.
 */
extern void init_gpios(void);

/**
* Initialization of buzzer from UDC v2_1 board. This function is ignored on
* v2_0.
*
* @param volume: value between 0 and 100 [%] to indicate buzzer volume.
*/
extern void init_buzzer(float volume);

/**
 * Initialization of EPWMSYNCI as general purpose digital input
 */
extern void cfg_epwmsynci_gpdi(void);

/**
 * Initialization of EPWMSYNCO as general purpose digital output
 */
extern void cfg_epwmsynco_gpdo(void);

#endif /* UDC_C28_H_ */
