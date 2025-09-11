/******************************************************************************
 * Copyright (C) 2020 by LNLS - Brazilian Synchrotron Light Laboratory
 *
 * Redistribution, modification or use of this software in source or binary
 * forms is permitted as long as the files maintain this copyright. LNLS and
 * the Brazilian Center for Research in Energy and Materials (CNPEM) are not
 * liable for any misuse of this material.
 *
 *****************************************************************************/

/**
 * @file scope.c
 * @brief Scope module
 *
 * This module implements functions for Scope functionality, which serves as a
 * configurable buffer for signal acquisition.
 *
 * @author gabriel.brunheira
 * @date 01/04/2020
 *
 */

#include "scope/scope.h"

void init_scope(scope_t *p_scp, float freq_base, float freq_sampling,
                float *p_buf_start, uint16_t size, float *p_source,
                void *p_run_scope)
{
    /// This function needs to run first to set "size" parameter, used by
    /// cfg_freq_scope()
    init_buffer(&p_scp->buffer, p_buf_start, size);

    init_timeslicer(&p_scp->timeslicer, freq_base);
    cfg_freq_scope(p_scp, freq_sampling);

    p_scp->p_source = p_source;
    p_scp->p_run_scope = p_run_scope;
}

void cfg_source_scope(scope_t *p_scp, float *p_source)
{
    p_scp->p_source = p_source;
}

void cfg_freq_scope(scope_t *p_scp, float freq_sampling)
{
    cfg_timeslicer(&p_scp->timeslicer, freq_sampling);
    p_scp->duration = ((float) size_buffer(&p_scp->buffer)) / p_scp->timeslicer.freq_sampling;
}

void cfg_duration_scope(scope_t *p_scp, float duration)
{
    float freq_sampling;

    freq_sampling = ((float) size_buffer(&p_scp->buffer)) / duration;
    cfg_freq_scope(p_scp, freq_sampling);
}

void enable_scope(scope_t *p_scp)
{
    enable_buffer(&p_scp->buffer);
}

void disable_scope(scope_t *p_scp)
{
    disable_buffer(&p_scp->buffer);
}

void reset_scope(scope_t *p_scp)
{
    reset_buffer(&p_scp->buffer);
}

void run_scope_shared_ram(scope_t *p_scp)
{
    insert_buffer(&p_scp->buffer, *p_scp->p_source);
}

/// TODO: Prototype for function which uses onboard RAM
void run_scope_onboard_ram(scope_t *p_scp)
{
}

void cfg_trig_delay_scope(scope_t *p_scp, float delay)
{
    if(delay > 100.0) delay = 100.0;
    if(delay < 0.0) delay = 0.0;

    p_scp->trig_delay = 1.0 - delay/100.0;
}

void trigger_scope(scope_t *p_scp)
{
    uint16_t buf_size = size_buffer(&p_scp->buffer);

    p_scp->buffer.p_buf_stop = p_scp->buffer.p_buf_idx - 1 +
                               ((uint16_t) (buf_size * p_scp->trig_delay));

    if(p_scp->buffer.p_buf_stop > p_scp->buffer.p_buf_end)
    {
        p_scp->buffer.p_buf_stop = p_scp->buffer.p_buf_stop - buf_size;
    }
    else if(p_scp->buffer.p_buf_stop < p_scp->buffer.p_buf_start)
    {
        p_scp->buffer.p_buf_stop = p_scp->buffer.p_buf_end;
    }

    postmortem_buffer(&p_scp->buffer);
}
