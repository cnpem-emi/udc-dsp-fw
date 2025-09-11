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
 * @file structs.c
 * @brief Common structs definitions.
 * 
 * Definition of structs and its functions, used on different applications.
 *
 * @author gabriel.brunheira
 * @date 25/10/2017
 *
 */

#include "structs.h"

/**
 * Initialization for an instance of ```buf_t```. It requires a pre-defined
 * ```float``` array, addressed by ```p_buf_start```
 *
 * @param p_buf pointer to buffer structure
 * @param p_buf_start pointer to the first element of the pre-defined array
 * @param size number of elements on the array used by the buffer
 */
void init_buffer(buf_t *p_buf, volatile float *p_buf_start, uint16_t size)
{
    p_buf->status = Disabled;
    p_buf->p_buf_start = p_buf_start;
    p_buf->p_buf_end = p_buf_start + size - 1;
    reset_buffer(p_buf);
}

/**
 * Set values from buffer to 0 and reset index pointer
 *
 * @param p_buf pointer to buffer structure
 */
void reset_buffer(buf_t *p_buf)
{
    p_buf->status = Disabled;
    p_buf->p_buf_idx = p_buf->p_buf_start;

    while(p_buf->p_buf_idx < p_buf->p_buf_end)
    {
        *(p_buf->p_buf_idx++) = 0.0;
    }

    p_buf->p_buf_idx = p_buf->p_buf_start;
}

/**
 * Enable specified buffer
 *
 * @param p_buf pointer to buffer structure
 */
void enable_buffer(buf_t *p_buf)
{
    //p_buf->status = Idle;
    p_buf->status = Buffering;
    p_buf->p_buf_stop = p_buf->p_buf_end + 1;
}

/**
 * Disable specified buffer
 *
 * @param p_buf pointer to buffer structure
 */
void disable_buffer(buf_t *p_buf)
{
    p_buf->status = Disabled;
    p_buf->p_buf_stop = p_buf->p_buf_end + 1;
}

/**
 * Trigger postmortem for specified buffer
 *
 * @param p_buf pointer to buffer structure
 */
void postmortem_buffer(buf_t *p_buf)
{
    p_buf->status = Postmortem;
}

/**
 * Return number of elements of specified buffer
 *
 * @param p_buf pointer to buffer structure
 * @return buffer size
 */
uint16_t size_buffer(buf_t *p_buf)
{
    static uint16_t size;
    size = p_buf->p_buf_end - p_buf->p_buf_start + 1;
    return size;
}

/**
 * Return position of current index pointer on specified buffer
 *
 * @param p_buf pointer to buffer structure
 * @return index position
 */
uint16_t idx_buffer(buf_t *p_buf)
{
    uint16_t idx;
    idx = p_buf->p_buf_idx - p_buf->p_buf_start;
    return idx;
}

/**
 * Insert new data to buffer. If buffer is enabled and full (```idx == end```),
 * it wraps around to the beginning, and keeps inserting new values. If buffer
 * is disabled, it stops to insert as soon as the buffer is filled.
 *
 * @param p_buf pointer to buffer structure
 * @param data new data to insert to buffer
 * @return indicate whether buffer is full
 */
uint16_t insert_buffer(buf_t *p_buf, float data)
{
    if( (p_buf->p_buf_idx >= p_buf->p_buf_start) &&
        (p_buf->p_buf_idx <= p_buf->p_buf_end) )
    {
        if(p_buf->status == Buffering)
        {
            *(p_buf->p_buf_idx) = data;

            if(p_buf->p_buf_idx++ == p_buf->p_buf_end)
            {
                p_buf->p_buf_idx = p_buf->p_buf_start;
            }
        }
        else if(p_buf->status == Postmortem)
        {
            *(p_buf->p_buf_idx) = data;

            if(p_buf->p_buf_idx == p_buf->p_buf_stop)
            {
                p_buf->status = Disabled;
            }

            if(p_buf->p_buf_idx++ == p_buf->p_buf_end)
            {
                p_buf->p_buf_idx = p_buf->p_buf_start;
                //p_buf->status = Disabled;
            }
        }
        else
        {
            //p_buf->status = Disabled;
        }
    }

    else
    {
        p_buf->p_buf_idx = p_buf->p_buf_start;
        p_buf->status = Idle;
    }

    return p_buf->status;
}


/**
 * Test to indicate whether the buffer contains any sample outside the limits
 * determined by the
 *
 * @param p_buf pointer to buffer structure
 * @param value reference value for test
 * @param tol tolerance value for test
 * @return
 */
uint16_t test_buffer_limits(buf_t *p_buf, float value, float tol)
{
    float samp;

    p_buf->p_buf_idx = p_buf->p_buf_start;

    while(p_buf->p_buf_idx <= p_buf->p_buf_end)
    {
        samp = *(p_buf->p_buf_idx++);

        if( (samp < value - tol) || (samp > value + tol) )
        {
            p_buf->p_buf_idx = p_buf->p_buf_start;
            return 1;
        }

    }

    p_buf->p_buf_idx = p_buf->p_buf_start;
    return 0;
}


/**
 * TODO: Put here the implementation for your private functions.
 */
