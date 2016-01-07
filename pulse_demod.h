/**
 * Pulse demodulation functions
 * 
 * Binary demodulators (PWM/PPM/Manchester/...) using a pulse data structure as input
 *
 * Copyright (C) 2015 Tommy Vestermark
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef INCLUDE_PULSE_DEMOD_H_
#define INCLUDE_PULSE_DEMOD_H_

#include <stdint.h>
#include "pulse_detect.h"
#include "bitbuffer.h"

static char ledState;

/// Demodulate a Pulse Code Modulation signal
///
/// Demodulate a Pulse Code Modulation (PCM) signal where bit width
/// is fixed and each bit starts with a pulse or not. It may be either
/// Return-to-Zero (RZ) encoding, where pulses are shorter than bit width
/// or Non-Return-to-Zero (NRZ) encoding, where pulses are equal to bit width
/// The presence of a pulse is:
/// - Presence of a pulse equals 1
/// - Absence of a pulse equals 0
/// @param device->short_limit: Nominal width of pulse [samples]
/// @param device->long_limit:  Nominal width of bit period [samples]
/// @param device->reset_limit: Maximum gap size before End Of Message [samples].
/// @return number of events processed
int pulse_demod_pcm(const pulse_data_t *pulses);
int demod_cc_bit_buffer(bitbuffer_t *bits);

#endif /* INCLUDE_PULSE_DEMOD_H_ */
