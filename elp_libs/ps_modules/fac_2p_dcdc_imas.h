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
 * @file fac_2p_dcdc_imas.h
 * @brief FAC-2p DC/DC Stage module
 * 
 * Module for control of two DC/DC modules of FAC power supplies.
 * It implements the controller for load current.
 *
 * PWM signals are mapped as the following :
 *
 *      ePWM  =>  Signal   ( POF transmitter)
 *     channel     Name    (    on BCB      )
 *
 *     ePWM1A => Q1_MOD_1        (PWM1)
 *     ePWM2A => Q2_MOD_1        (PWM3)
 *     ePWM3A => Q1_MOD_2        (PWM5)
 *     ePWM4A => Q2_MOD_2        (PWM7)
 *
 *  TODO: Include reference filtering and feedforward, capacitor banks voltage
 *  feedforward and modules output voltage share control.
 *
 * @author gabriel.brunheira
 * @date 27/02/2019
 *
 */

#ifndef FAC_2P_DCDC_IMAS_H_
#define FAC_2P_DCDC_IMAS_H_

extern void main_fac_2p_dcdc_imas(void);

#endif /* FAC_2P_DCDC_IMAS_H_ */
