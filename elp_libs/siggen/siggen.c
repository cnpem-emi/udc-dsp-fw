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
 * @file siggen.c
 * @brief Signal Generator module
 *
 * This module implements a real-time parametric digital signal generator. It
 * supports some broadly used signals, like sinusoidals, trapezoids, squares,
 * triangular, etc.
 *
 * @author gabriel.brunheira
 * @date 11/02/2018
 *
 * TODO: implement square, triangular, frequency sweep and PRBS signals
 *
 */

#include <math.h>
#include <float.h>
#include "siggen.h"

#define _USE_MATH_DEFINES
#define PI  3.14159265358979323846

const static float default_aux_param[NUM_SIGGEN_AUX_PARAM] = {0};

#pragma CODE_SECTION(set_siggen_freq, "ramfuncs");
#pragma CODE_SECTION(enable_siggen, "ramfuncs");
#pragma CODE_SECTION(disable_siggen, "ramfuncs");
#pragma CODE_SECTION(run_siggen_sine, "ramfuncs");
#pragma CODE_SECTION(run_siggen_dampedsine, "ramfuncs");
#pragma CODE_SECTION(run_siggen_trapezoidal, "ramfuncs");

void init_siggen(siggen_t *p_siggen, float freq_sampling, volatile float *p_out);
void cfg_siggen(siggen_t *p_siggen, siggen_type_t sig_type, uint16_t num_cycles,
                float freq, float amplitude, float offset, float *p_aux_param);
void scale_siggen(siggen_t *p_siggen, float amplitude, float offset);
void set_siggen_freq(siggen_t *p_siggen);

void enable_siggen(siggen_t *p_siggen);
void disable_siggen(siggen_t *p_siggen);
void reset_siggen(siggen_t *p_siggen);

void run_siggen_sine(siggen_t *p_siggen);
void run_siggen_dampedsine(siggen_t *p_siggen);
void run_siggen_trapezoidal(siggen_t *p_siggen);

/**
 * Initialization of Signal Generator module. SigGen must be disabled.
 *
 * @param p_siggen      Pointer to specified siggen controller
 * @param freq_sampling Sampling frequency [Hz]
 * @param p_out         Pointer to output
 */
void init_siggen(siggen_t *p_siggen, float freq_sampling, volatile float *p_out)
{
	if(p_siggen->enable == 0)
	{
	    p_siggen->freq_sampling = freq_sampling;
	    cfg_siggen(p_siggen,Sine,1,1.0,1.0,0.0, default_aux_param);
        p_siggen->p_out = p_out;
	}
}

/**
 * Configuration of generated signal. SigGen must be disabled. For continuous
 * operation of the signal, num_cycles = 0. In this case (except for
 * Trapezoidal), frequency is rounded off to nearest integer. To generate
 * continuous-like operation with fractional frequencies, use a high value for
 * "num_cycles" parameter.
 *
 * @param p_siggen      Pointer to specified siggen controller
 * @param sig_type      Signal type
 * @param num_cycles    Number of period cycles.
 * @param freq          Frequency [Hz]
 * @param amplitude     Amplitude [A/V/%]
 * @param offset        Offset [A/V/%]
 * @param p_aux_param   Additional parameters, related to signal type
 */
void cfg_siggen(siggen_t *p_siggen, siggen_type_t sig_type, uint16_t num_cycles,
                float freq, float amplitude, float offset, float *p_aux_param)
{
    uint16_t i;

    if(p_siggen->enable == 0)
    {
        p_siggen->type = sig_type;
        p_siggen->num_cycles = num_cycles;
        p_siggen->n = 0.0;
        p_siggen->freq = freq;

        scale_siggen(p_siggen, amplitude, offset);
        set_siggen_freq(p_siggen);

        for(i = 0; i < NUM_SIGGEN_AUX_PARAM; i++)
        {
            p_siggen->aux_param[i] = p_aux_param[i];
        }

        for(i = 0; i < NUM_SIGGEN_AUX_VAR; i++)
        {
            p_siggen->aux_var[i] = 0.0;
        }

        switch(sig_type)
        {
            case Sine:
            {
                /// Sample phase
                p_siggen->aux_var[1] = PI * p_siggen->aux_param[0] / 180.0;

                /// Total number of samples (apply only for fractional frequencies)
                p_siggen->aux_var[2] = p_siggen->num_cycles +
                                       ( p_siggen->aux_param[1] -
                                         p_siggen->aux_param[0] ) / (360.0);
                if(p_siggen->aux_param[0] > p_siggen->aux_param[1])
                {
                    p_siggen->aux_var[2]++;
                }
                p_siggen->aux_var[2] *= p_siggen->freq_sampling/(p_siggen->freq);

                p_siggen->p_run_siggen = &run_siggen_sine;
                break;
            }

            case DampedSine:
            {
                /// Sample phase
                p_siggen->aux_var[1] = PI * p_siggen->aux_param[0] / 180.0;

                /// Total number of samples (apply only for fractional frequencies)
                p_siggen->aux_var[2] = p_siggen->num_cycles +
                                       ( p_siggen->aux_param[1] -
                                         p_siggen->aux_param[0] ) / (360.0);
                if(p_siggen->aux_param[0] > p_siggen->aux_param[1])
                {
                    p_siggen->aux_var[2]++;
                }
                p_siggen->aux_var[2] *= p_siggen->freq_sampling/(p_siggen->freq);

                /// Damping exponencial coefficient
                p_siggen->aux_var[3] = -(1.0/p_siggen->aux_param[2]) /
                                         p_siggen->freq_sampling ;

                p_siggen->p_run_siggen = &run_siggen_dampedsine;
                break;
            }
            case Trapezoidal:
            {
                p_siggen->aux_var[0] = p_siggen->aux_param[0] *
                                       p_siggen->freq_sampling;

                p_siggen->aux_var[1] = (p_siggen->aux_param[0] +
                                        p_siggen->aux_param[1]) *
                                        p_siggen->freq_sampling;

                p_siggen->aux_var[2] = (p_siggen->aux_param[0] +
                                        p_siggen->aux_param[1] +
                                        p_siggen->aux_param[2]) *
                                        p_siggen->freq_sampling;

                p_siggen->aux_var[3] = amplitude / p_siggen->aux_var[0];

                p_siggen->aux_var[4] = amplitude / (p_siggen->aux_param[2] *
                                       p_siggen->freq_sampling);

                p_siggen->aux_var[5] = 0.0;

                p_siggen->p_run_siggen = &run_siggen_trapezoidal;
                break;
            }
        }
    }
}

/**
 * Adjust amplitude and offset of signal.
 *
 * @param p_siggen  Pointer to specified siggen controller
 * @param amplitude Amplitude [A/V/%]
 * @param offset    Offset [A/V/%]
 */
void scale_siggen(siggen_t *p_siggen, float amplitude, float offset)
{
    p_siggen->amplitude = amplitude;
    p_siggen->offset = offset;
}

/**
 * Set frequency of signal. Case num_cycle = 0, it rounds off to nearest
 * interger.
 *
 * @param p_siggen  Pointer to specified siggen controller
 */
void set_siggen_freq(siggen_t *p_siggen)
{
    switch(p_siggen->type)
    {
        case Sine:
        case DampedSine:
        {
            /// Continuous operation only allows integer frequencies. To
            /// generate continuous-like operation with fractional frequencies,
            /// use a high value for "num_cycles" parameter
            if(p_siggen->num_cycles == 0)
            {
                p_siggen->freq = fabs(roundf(p_siggen->freq));
            }

            p_siggen->aux_var[0] = 2.0 * PI * (p_siggen->freq) /
                                   p_siggen->freq_sampling;
            break;
        }

        default:
        {
            p_siggen->freq = 0.0;
            break;
        }
    }
}

/**
 * Enable Signal Generator.
 *
 * @param p_siggen  Pointer to specified siggen controller
 */
void enable_siggen(siggen_t *p_siggen)
{
    if(p_siggen->enable == 0)
    {
        reset_siggen(p_siggen);
        switch(p_siggen->type)
        {
            case Sine:
            case DampedSine:
                set_siggen_freq(p_siggen);
                break;

            default:
                break;
        }
        p_siggen->enable = 1;
    }
}

/**
 * Disable Signal Generator.
 *
 * @param p_siggen  Pointer to specified siggen controller
 */
void disable_siggen(siggen_t *p_siggen)
{
    p_siggen->enable = 0;
}

/**
 * Reset Signal Generator.
 *
 * @param p_siggen  Pointer to specified siggen controller
 */
void reset_siggen(siggen_t *p_siggen)
{
    p_siggen->n = 0.0;
    //*(p_siggen->p_out) = 0.0;
}

/**
 * Run sinusoidal signal.
 *
 * @param p_siggen  Pointer to specified siggen controller
 */
void run_siggen_sine(siggen_t *p_siggen)
{
	if(p_siggen->enable)
	{
		*(p_siggen->p_out) = (p_siggen->amplitude) * sin( p_siggen->aux_var[0] *
		                      p_siggen->n++ + p_siggen->aux_var[1]) +
		                      p_siggen->offset;

		if(p_siggen->aux_var[2] >  0)
		{
			if(p_siggen->n >= p_siggen->aux_var[2])
			{
				disable_siggen(p_siggen);
			}

		}
		else if(p_siggen->n >= p_siggen->freq_sampling)
		{
			/**
			 *  Compares with freq_sampling, in order to increment n during 1
			 *  second. If n is compared with aux_var[2], signal is generated
			 *  discontinuously, since aux_var[2] could result in float, while
			 *  n doesn't.
			 *
			 *  After counting up to 1 second, frequency parameter is updated,
			 *  creating a smooth transition from one frequency to other.
			 *
			 */
			set_siggen_freq(p_siggen);
			p_siggen->n = 0.0;
		}
	}
}

/**
 * Run damped sinusoidal signal.
 *
 * @param p_siggen  Pointer to specified siggen controller
 */
void run_siggen_dampedsine(siggen_t *p_siggen)
{
	if(p_siggen->enable)
	{
		if(p_siggen->n < p_siggen->aux_var[2])
		{
			*(p_siggen->p_out) = p_siggen->amplitude *
			                     exp(p_siggen->aux_var[3] * p_siggen->n) *
			                     sin( p_siggen->aux_var[0] * p_siggen->n +
			                     p_siggen->aux_var[1] ) + p_siggen->offset;

			p_siggen->n++;
		}
		else
		{
			disable_siggen(p_siggen);
		}
	}
}

/**
 * Run trapezoidal signal.
 *
 * @param p_siggen  Pointer to specified siggen controller
 */
void run_siggen_trapezoidal(siggen_t *p_siggen)
{
	if(p_siggen->enable)
	{
		if(p_siggen->aux_var[5] < p_siggen->num_cycles)
		{
			if(p_siggen->n < p_siggen->aux_var[0])
			{
				*(p_siggen->p_out) = p_siggen->n * p_siggen->aux_var[3] +
				                    (p_siggen->offset);
			}
			else if(p_siggen->n < p_siggen->aux_var[1])
			{
				*(p_siggen->p_out) = (p_siggen->amplitude) + (p_siggen->offset);
			}
			else if(p_siggen->n < p_siggen->aux_var[2])
			{
				*(p_siggen->p_out) = p_siggen->aux_var[4] *
				                     (p_siggen->aux_var[1] - p_siggen->n) +
				                     p_siggen->amplitude + p_siggen->offset;
			}
			else
			{
				*(p_siggen->p_out) = p_siggen->offset;
				p_siggen->aux_var[5]++;
				p_siggen->n = 0.0;
			}
			p_siggen->n++;
		}
		else
		{
			disable_siggen(p_siggen);
			p_siggen->aux_var[5]= 0;
		}
	}
}
