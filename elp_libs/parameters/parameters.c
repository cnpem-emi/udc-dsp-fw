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
 * @file parameters.c
 * @brief Power supply parameters bank module.
 * 
 * This module implements a data structure for initialization and configuration
 * of parameters for operation of the power supplies applications.
 *
 * @author gabriel.brunheira
 * @date 23/02/2018
 *
 */

#include <string.h>
#include "parameters.h"
#include "ipc/ipc.h"

volatile param_t g_parameters[NUM_MAX_PARAMETERS];

void init_param(param_id_t id, param_type_t type, uint16_t num_elements,
                uint16_t *p_param)
{
    uint16_t n;

    if(num_elements > 0)
    {
        g_parameters[id].id = id;
        g_parameters[id].type = type;
        g_parameters[id].num_elements = num_elements;
        g_parameters[id].p_val.u16 = p_param;
    }
}

uint16_t set_param(param_id_t id, uint16_t n, float val)
{
    if(n < g_parameters[id].num_elements)
    {
        switch(g_parameters[id].type)
        {
            case is_uint16_t:
            {
                *(g_parameters[id].p_val.u16 + n) = (uint16_t) val;
                break;
            }

            case is_uint32_t:
            {
                *(g_parameters[id].p_val.u32 + n) = (uint32_t) val;
                break;
            }

            case is_float:
            {
                *(g_parameters[id].p_val.f + n) = val;
                break;
            }

            default:
            {
                return 0;
            }
        }

        return 1;
    }
    else
    {
        return 0;
    }
}

float get_param(param_id_t id, uint16_t n)
{
    if(n < g_parameters[id].num_elements)
    {
        switch(g_parameters[id].type)
        {
            case is_uint16_t:
            {
                return (float) *(g_parameters[id].p_val.u16 + n);
            }

            case is_uint32_t:
            {
                return (float) *(g_parameters[id].p_val.u32 + n);
            }

            case is_float:
            {
                return *(g_parameters[id].p_val.f + n);
            }

            default:
            {
                return NAN;
            }
        }
    }
    else
    {
        return NAN;
    }
}

void init_parameters_bank(void)
{
    init_param(SigGen_Type, is_uint16_t, 1,
               (uint16_t *) &g_ipc_mtoc.siggen.type);
    init_param(SigGen_Num_Cycles, is_uint16_t, 1,
               (uint16_t *) &g_ipc_mtoc.siggen.num_cycles);
    init_param(SigGen_Freq, is_float, 1,
               (uint16_t *) &g_ipc_mtoc.siggen.freq);
    init_param(SigGen_Amplitude, is_float, 1,
               (uint16_t *)  &g_ipc_mtoc.siggen.amplitude);
    init_param(SigGen_Offset, is_float, 1,
               (uint16_t *) &g_ipc_mtoc.siggen.offset);
    init_param(SigGen_Aux_Param, is_float, NUM_SIGGEN_AUX_PARAM,
               (uint16_t *) &g_ipc_mtoc.siggen.aux_param[0]);

    init_param(WfmRef_ID_WfmRef, is_uint16_t, 1,
               (uint16_t *) &g_ipc_mtoc.wfmref.wfmref_selected);
    init_param(WfmRef_SyncMode, is_uint16_t, 1,
               (uint16_t *) &g_ipc_mtoc.wfmref.sync_mode);
    init_param(WfmRef_Gain, is_float, 1,
               (uint16_t *) &g_ipc_mtoc.wfmref.gain);
    init_param(WfmRef_Offset, is_float, 1,
               (uint16_t *) &g_ipc_mtoc.wfmref.offset);
}
