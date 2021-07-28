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
 * @file event_manager.c
 * @brief Event manager module
 *
 * This module is responsible for the management of events during power supplies
 * operation, including data log on onboard memory. An event is generated by any
 * of the following situations:
 *
 *      1. Interlocks
 *      2. Alarms, in general caused by unusual operation values
 *      3. Commands received via communication interfaces, such as turn on/off,
 *         selection of operation mode, open/close control loop, changes on
 *         setpoint or other operation paramenters, etc. Usually is done by BSMP
 *         functions or HMI operation.
 *
 * Current version implements only interlocks management, including debouncing
 * logic.
 *
 * TODO: Events based on alarms and commands, and data log.
 * 
 * @author gabriel.brunheira
 * @date 14/08/2018
 *
 */

#include <stdint.h>
#include "boards/udc_c28.h"
#include "event_manager/event_manager.h"
#include "ipc/ipc.h"

/**
 * Maximum debouncing parameters
 */
#define MAX_DEBOUNCE_TIME_US    5000000
#define MAX_RESET_TIME_US       10000000

/**
 * Private variables
 */

/// Look-up-table to convert bit-position to bit mask
const static uint32_t lut_bit_position[32] =
{
    0x00000001, 0x00000002, 0x00000004, 0x00000008,
    0x00000010, 0x00000020, 0x00000040, 0x00000080,
    0x00000100, 0x00000200, 0x00000400, 0x00000800,
    0x00001000, 0x00002000, 0x00004000, 0x00008000,
    0x00010000, 0x00020000, 0x00040000, 0x00080000,
    0x00100000, 0x00200000, 0x00400000, 0x00800000,
    0x01000000, 0x02000000, 0x04000000, 0x08000000,
    0x10000000, 0x20000000, 0x40000000, 0x80000000
};

/**
 * Public variables
 */
volatile event_manager_t g_event_manager[NUM_MAX_PS_MODULES];

#pragma CODE_SECTION(set_hard_interlock, "ramfuncs");
#pragma CODE_SECTION(set_soft_interlock, "ramfuncs");
#pragma CODE_SECTION(isr_hard_interlock, "ramfuncs");
#pragma CODE_SECTION(isr_soft_interlock, "ramfuncs");

/**
 * Initialization of specified event manager. There is a separate event manager
 * for each power supply/module. It should be noted that the debounce logic
 * uses a fixed-period event (like the controller ISR) as time-base, so all
 * debounce times are integer multiples of this period.
 *
 * @param id id of event manager specific of a power supply/module
 * @param freq_timebase time-base frequecy, from which debounce timing is generated [Hz]
 * @param num_hard_itlks number of hard interlocks specified by a ps_module
 * @param num_soft_itlks number of soft interlocks specified by a ps_module
 * @param p_hard_itlks_debounce_time_us pointer to array of debounce time of hard interlocks [us]
 * @param p_hard_itlks_reset_time_us pointer to array of reset time of hard interlocks [us]
 * @param p_soft_itlks_debounce_time_us pointer to array of debounce time of soft interlocks [us]
 * @param p_soft_itlks_reset_time_us pointer to array of debounce time of soft interlocks [us]
 */
void init_event_manager(uint16_t id, float freq_timebase,
                        uint16_t num_hard_itlks, uint16_t num_soft_itlks,
                        uint32_t *p_hard_itlks_debounce_time_us,
                        uint32_t *p_hard_itlks_reset_time_us,
                        uint32_t *p_soft_itlks_debounce_time_us,
                        uint32_t *p_soft_itlks_reset_time_us)
{
    uint16_t i;
    uint32_t debounce_time_us, reset_time_us, max_reset_counts;

    max_reset_counts = (uint32_t) ((freq_timebase * MAX_RESET_TIME_US) * 1e-6);

    g_event_manager[id].timebase_flag = 0;
    g_event_manager[id].freq_timebase = freq_timebase;
    g_event_manager[id].hard_interlocks.num_events = num_hard_itlks;
    g_event_manager[id].soft_interlocks.num_events = num_soft_itlks;

    for(i = 0; i < NUM_MAX_EVENT_COUNTER; i++)
    {
        g_event_manager[id].hard_interlocks.event[i].flag = 0;
        g_event_manager[id].hard_interlocks.event[i].counter = 0;

        /// Initialization of hard interlocks
        if(i < num_hard_itlks)
        {
            debounce_time_us = *(p_hard_itlks_debounce_time_us + i);
            reset_time_us = *(p_hard_itlks_reset_time_us+i);

            /** Prevents bypassing a interlock by setting a very large debounce
             * time
             */
            SATURATE(debounce_time_us, MAX_DEBOUNCE_TIME_US , 0);

            g_event_manager[id].hard_interlocks.event[i].debounce_count =
                    (uint32_t) ( (freq_timebase * debounce_time_us) * 1e-6);
            g_event_manager[id].hard_interlocks.event[i].reset_count =
                    (uint32_t) ( (freq_timebase * reset_time_us) * 1e-6);

            /**
             *  Prevents bypassing an interlock by setting a reset time smaller
             *  than debounce time.
             */
            SATURATE(g_event_manager[id].hard_interlocks.event[i].reset_count,
                     max_reset_counts,
                     g_event_manager[id].hard_interlocks.event[i].debounce_count+1);

        }
        else
        {
            g_event_manager[id].hard_interlocks.event[i].debounce_count = 0;
            g_event_manager[id].hard_interlocks.event[i].reset_count = 0;
        }

        /// Initialization of soft interlocks
        g_event_manager[id].soft_interlocks.event[i].flag = 0;
        g_event_manager[id].soft_interlocks.event[i].counter = 0;

        if(i < num_soft_itlks)
        {
            debounce_time_us = *(p_soft_itlks_debounce_time_us + i);
            reset_time_us = *(p_soft_itlks_reset_time_us+i);

            /** Prevents bypassing a interlock by setting a very large debounce
             * time
             */
            SATURATE(debounce_time_us, MAX_DEBOUNCE_TIME_US , 0);

            g_event_manager[id].soft_interlocks.event[i].debounce_count =
                    (uint32_t) ( (freq_timebase * debounce_time_us) * 1e-6);
            g_event_manager[id].soft_interlocks.event[i].reset_count =
                    (uint32_t) ( (freq_timebase * reset_time_us) * 1e-6);

            /**
             *  Prevents bypassing an interlock by setting a reset time smaller
             *  than debounce time.
             */
            SATURATE(g_event_manager[id].soft_interlocks.event[i].reset_count,
                     max_reset_counts,
                     g_event_manager[id].soft_interlocks.event[i].debounce_count+1);
        }
        else
        {
            g_event_manager[id].soft_interlocks.event[i].debounce_count = 0;
            g_event_manager[id].soft_interlocks.event[i].reset_count = 0;
        }
    }
}

/**
 * Run debounce logic of interlocks for specified power supply/module. It checks
 * whether a time-base period has occured using timebase_flag, than increments
 * debounce counters for flagged interlocks. If a counter exceeds its reset
 * value (reset_time) before the interlock condition remains for sufficient time
 * (debounce_time), it resets. This function must be called at a higher
 * frequency than the time-base, for example, inside a background while loop.
 *
 * @param id id of event manager specific of a power supply/module
 */
void run_interlocks_debouncing(uint16_t id)
{
    uint16_t i;

    /// Check once per time-base period indicated by this flag
    if(g_event_manager[id].timebase_flag)
    {
        for(i = 0; i < g_event_manager[id].hard_interlocks.num_events; i++)
        {
            if( g_event_manager[id].hard_interlocks.event[i].flag )
            {
                if(++g_event_manager[id].hard_interlocks.event[i].counter >=
                     g_event_manager[id].hard_interlocks.event[i].reset_count)
                {
                    g_event_manager[id].hard_interlocks.event[i].flag = 0;
                    g_event_manager[id].hard_interlocks.event[i].counter = 0;
                }
            }
        }

        for(i = 0; i < g_event_manager[id].soft_interlocks.num_events; i++)
        {
            if( g_event_manager[id].soft_interlocks.event[i].flag )
            {
                if(++g_event_manager[id].soft_interlocks.event[i].counter >=
                     g_event_manager[id].soft_interlocks.event[i].reset_count)
                {
                    g_event_manager[id].soft_interlocks.event[i].flag = 0;
                    g_event_manager[id].soft_interlocks.event[i].counter = 0;
                }
            }
        }

        g_event_manager[id].timebase_flag = 0;
    }
}

/**
 * Set specified hard interlock for specified module. First, it sets a flag to
 * enable counter (incremented at each time-base period), and if it reaches
 * the debounce count, interlock is setted.
 *
 * @param id id of event manager specific of a power supply/module
 * @param itlk specified hard interlock
 */
void set_hard_interlock(uint16_t id, uint32_t itlk)
{
    // Protection against inexistent interlock
    if(itlk < g_event_manager[id].hard_interlocks.num_events)
    {
        g_event_manager[id].hard_interlocks.event[itlk].flag = 1;

        if(g_event_manager[id].hard_interlocks.event[itlk].counter >=
           g_event_manager[id].hard_interlocks.event[itlk].debounce_count)
        {
            if(!(g_ipc_ctom.ps_module[id].ps_hard_interlock & lut_bit_position[itlk]))
            {
                #ifdef USE_ITLK
                g_ipc_ctom.ps_module[id].turn_off(id);
                g_ipc_ctom.ps_module[id].ps_status.bit.state = Interlock;
                #endif

                g_ipc_ctom.ps_module[id].ps_hard_interlock |= lut_bit_position[itlk];
            }

            g_event_manager[id].hard_interlocks.event[itlk].flag = 0;
            g_event_manager[id].hard_interlocks.event[itlk].counter = 0;
        }
    }
}

/**
 * Set specified soft interlock for specified module. First, it sets a flag to
 * enable counter (incremented at each time-base period), and if it reaches
 * the debounce count, interlock is setted.
 *
 * @param id id of event manager specific of a power supply/module
 * @param itlk specified soft interlock
 */
void set_soft_interlock(uint16_t id, uint32_t itlk)
{
    // Protection against inexistent interlock
    if(itlk < g_event_manager[id].soft_interlocks.num_events)
    {
        g_event_manager[id].soft_interlocks.event[itlk].flag = 1;

        if(g_event_manager[id].soft_interlocks.event[itlk].counter >=
           g_event_manager[id].soft_interlocks.event[itlk].debounce_count)
        {
            if(!(g_ipc_ctom.ps_module[id].ps_soft_interlock & lut_bit_position[itlk]))
            {
                #ifdef USE_ITLK
                g_ipc_ctom.ps_module[id].turn_off(id);
                g_ipc_ctom.ps_module[id].ps_status.bit.state = Interlock;
                #endif

                g_ipc_ctom.ps_module[id].ps_soft_interlock |= lut_bit_position[itlk];
            }

            g_event_manager[id].soft_interlocks.event[itlk].flag = 0;
            g_event_manager[id].soft_interlocks.event[itlk].counter = 0;
        }
    }
}

/**
 * ISR for MtoC hard interlock request. This function re-uses set_hard_interlock()
 * function implementation for debouncing.
 *
 * It's important to guarantee that ARM uses the interlock register
 * (MtoC ps_hard_interlock) as the enumerate argument 'itlk' from
 * set_hard_interlock(), which indicates the most current activated interlock.
 *
 * In older version, it was used just like the C28 interlock registers, and in
 * this case, C28 would need to log2() this register to find out which bit
 * (or event) was activated for debouncing logic.
 *
 * Thus, in order to maintain efficient communication and simplify debouncing
 * logic, both ARM interlocks registers must be used differently from C28
 * interlock registers.
 */
interrupt void isr_hard_interlock(void)
{
    set_hard_interlock(g_ipc_mtoc.msg_id,
                       g_ipc_mtoc.ps_module[g_ipc_mtoc.msg_id].ps_hard_interlock);

    CtoMIpcRegs.MTOCIPCACK.all = HARD_INTERLOCK;
    PieCtrlRegs.PIEACK.all |= M_INT11;
}

/**
 * ISR for MtoC soft interlock request. This function re-uses set_soft_interlock()
 * function implementation for debouncing.
 *
 * It's important to guarantee that ARM uses the interlock register
 * (MtoC ps_soft_interlock) as the enumerate argument 'itlk' from
 * set_soft_interlock(), which indicates the most current activated interlock.
 *
 * In older version, it was used just like the C28 interlock registers, and in
 * this case, C28 would need to log2() this register to find out which bit
 * (or event) was activated for debouncing logic.
 *
 * Thus, in order to maintain efficient communication and simplify debouncing
 * logic, both ARM interlocks registers must be used differently from C28
 * interlock registers.
 */
interrupt void isr_soft_interlock(void)
{
    set_soft_interlock(g_ipc_mtoc.msg_id,
                       g_ipc_mtoc.ps_module[g_ipc_mtoc.msg_id].ps_soft_interlock);

    CtoMIpcRegs.MTOCIPCACK.all = SOFT_INTERLOCK;
    PieCtrlRegs.PIEACK.all |= M_INT11;
}

interrupt void isr_interlocks_timebase(void)
{
    SET_INTERLOCKS_TIMEBASE_FLAG(0);
    SET_INTERLOCKS_TIMEBASE_FLAG(1);
    SET_INTERLOCKS_TIMEBASE_FLAG(2);
    SET_INTERLOCKS_TIMEBASE_FLAG(3);

    PieCtrlRegs.PIEACK.all |= PIEACK_GROUP1;
}
