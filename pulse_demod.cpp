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

#include "pulse_demod.h"
#include "Logging.h"
#include "util.h"
#include "PowerPi.h"
#include <stdio.h>
#include <stdlib.h>

int pulse_demod_pcm(const pulse_data_t *pulses)
{
	int events = 0;
	bitbuffer_t bits = {0};
	const int MAX_ZEROS = 2000 / 64;
	const int TOLERANCE = 64 / 4;		// Tolerance is ±25% of a bit period

	for(unsigned n = 0; n < pulses->num_pulses; ++n) {
		// Determine number of high bit periods for NRZ coding, where bits may not be separated
		int highs = (pulses->pulse[n] + 64/2) / 64;
		// Determine number of bit periods in current pulse/gap length (rounded)
		int periods = (pulses->pulse[n] + pulses->gap[n] + 64/2) / 64;

		// Add run of ones (1 for RZ, many for NRZ)
		for (int i=0; i < highs; ++i) {
			bitbuffer_add_bit(&bits, 1);
		}
		// Add run of zeros
		periods -= highs;					// Remove 1s from whole period
		periods = min(periods, MAX_ZEROS); 	// Dont overflow at end of message
		for (int i=0; i < periods; ++i) {
			bitbuffer_add_bit(&bits, 0);
		}

		// End of Message?
		if (((n == pulses->num_pulses-1) 	// No more pulses? (FSK)
		 || (pulses->gap[n] > (unsigned)2000))	// Loong silence (OOK)
		 && (bits.bits_per_row[0] > 0)		// Only if data has been accumulated
		) {
			/*if (device->callback) {
				events += device->callback(&bits);
			}*/
			//bitbuffer_print(&bits);
			demod_cc_bit_buffer(&bits);
			bitbuffer_clear(&bits);
		}
	} // for
	return events;
}

int demod_cc_bit_buffer(bitbuffer_t *bitbuffer)
{
	uint8_t init_pattern[] = {
		0b11001100, //8
		0b11001100, //16
		0b11001100, //24
		0b11001110, //32
		0b10010001, //40
		0b01011101, //45 (! last 3 bits is not init)
	};
	unsigned int start_pos = bitbuffer_search(bitbuffer, 0, 0, init_pattern, 45);

	if (start_pos == bitbuffer->bits_per_row[0]){
		return 0;
	}
	start_pos += 45;

	bitbuffer_t packet_bits = { 0 };

	start_pos = bitbuffer_manchester_decode(bitbuffer, 0, start_pos, &packet_bits, 0);
	uint8_t *packet = packet_bits.bb[0];

	/*tmp.PairingRequest = ((packet[0] >> 7) & 1) == 1;
			tmp.DataSensor = ((packet[0] >> 6) & 1) == 1;
			tmp.DeviceAddress = (ushort)(((packet[0] & 15) << 8) | packet[1]);
			tmp.Sensor1Valid = ((packet[2] >> 7) & 1) == 1;
			tmp.Sensor1Value = (ushort)(((packet[2] & 127) << 8) | packet[3]);
			tmp.Sensor2Valid = ((packet[4] >> 7) & 1) == 1;
			tmp.Sensor2Value = (ushort)(((packet[4] & 127) << 8) | packet[5]);
			tmp.Sensor3Valid = ((packet[6] >> 7) & 1) == 1;
			tmp.Sensor3Value = (ushort)(((packet[6] & 127) << 8) | packet[7]);
			tmp.CaptureTime = DateTime.Now;
			*/

	bool pairing =  ((packet[0] >> 7) & 1) == 1;
	uint16_t address = ((packet[0] & 15) << 8) | packet[1];
	bool sensor1Valid = ((packet[2] >> 7) & 1) == 1;
	uint16_t sensor1Value = ((packet[2] & 127) << 8) | packet[3];

	char devName[10];
	sprintf(devName, "device_%03X", address);
	
	if (sensor1Valid)
	{
		send_to_emon(devName, sensor1Value);
		
		if (Verbose)
		{
			fprintf(stderr, "Current cost address: device_%03X%s %u W \n", address, (pairing ? "*" : " "), sensor1Value);
		}
	}	

	return 1;
}
