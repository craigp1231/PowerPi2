#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include "Logging.h"
#include "PowerPi.h"
#include "rtl-sdr.h"
#include "baseband.h"
#include "pulse_detect.h"
#include "pulse_demod.h"

static int do_exit = 0;
static int do_exit_async = 0, frequencies = 0, events = 0;
static rtlsdr_dev_t *dev = NULL;
uint32_t samp_rate = DEFAULT_SAMPLE_RATE;
uint32_t freq = DEFAULT_FREQUENCY;

bool Verbose;
bool TimeFromPi;
char EmonKey[33];

struct sample
{
	int8_t Real;
	int8_t Imag;
};

struct dm_state {
	sample *samples;
	int32_t level_limit;
	int16_t am_buf[MAXIMAL_BUF_LENGTH];	// AM demodulated signal (for OOK decoding)
	union {
		// These buffers aren't used at the same time, so let's use a union to save some memory
		int16_t fm_buf[MAXIMAL_BUF_LENGTH];	// FM demodulated signal (for FSK decoding)
		uint16_t temp_buf[MAXIMAL_BUF_LENGTH];	// Temporary buffer (to be optimized out..)
	};

	FilterState lowpass_filter_state;
	DemodFM_State demod_FM_state;

	pulse_data_t	pulse_data;
	pulse_data_t	fsk_pulse_data;

	int sampleCount;
};

static void rtlsdr_callback(unsigned char *buf, uint32_t len, void *ctx)
{
	struct dm_state *demod = (dm_state *)ctx;
	//signed char *buffer = (signed char *)buf;
	//uint16_t* sbuf = (uint16_t*)buf;

	if (do_exit || do_exit_async)
		return;

	// AM demodulation
	envelope_detect(buf, demod->temp_buf, len / 2);
	baseband_low_pass_filter(demod->temp_buf, demod->am_buf, len / 2, &demod->lowpass_filter_state);

	// FM demodulation
	baseband_demod_FM(buf, demod->fm_buf, len / 2, &demod->demod_FM_state);

	// There could be more than one package in the callback
	int package_type = 1;	// Just to get us started
	while (package_type)
	{
		package_type = detect_pulse_package(demod->am_buf, demod->fm_buf, len / 2, demod->level_limit, samp_rate, &demod->pulse_data, &demod->fsk_pulse_data);

		// FSK package detected
		if (package_type == 2)
		{
			pulse_demod_pcm(&demod->fsk_pulse_data);
		}
	}
}

static void sighandler(int signum) {
	if (signum == SIGPIPE) {
		signal(SIGPIPE, SIG_IGN);
	}
	else {
		fprintf(stderr, "Signal caught, exiting!\n");
	}
	do_exit = 1;
	rtlsdr_cancel_async(dev);
}

int main(int argc, char *argv[])
{
	int device_count, i, r, j;
	char vendor[256], product[256], serial[256];
	uint32_t dev_index = 0;
	struct sigaction sigact;
	uint32_t out_block_size = DEFAULT_BUF_LENGTH;
	uint8_t *buffer;
	struct dm_state* demod;
	
	// Process arguments
	for (j = 1; j < argc; j++)
	{
		int more = j + 1 < argc; /* There are more arguments. */
		
		if (!strcmp(argv[j], "-v"))
		{
			Verbose = true;
		}
		else if (!strcmp(argv[j], "-t"))
		{
			TimeFromPi = true;
		}
	}
	
	// Read Emon CMS key (if it exists)
	int pos = 0, c = 0;
	FILE *f = fopen("/etc/emon.key", "r");
	if (f)
	{
		do
		{
			c = fgetc(f);
			if (c != EOF) EmonKey[pos++] = (char)c;
		} while (c != EOF && c != '\n' && pos < 32);
		
		fclose(f);
		
		fprintf(stderr, "Emon Key: %s\n", EmonKey);
	}
	else
	{
		fprintf(stderr, "/etc/emon.key file not found, will not log to Emon CMS.\n");
	}
	

	demod = (dm_state*)malloc(sizeof(struct dm_state));
	memset(demod, 0, sizeof(struct dm_state));

	baseband_init();

	demod->samples = (sample*)malloc(sizeof(sample) * DEFAULT_MAX_SAMPLES);
	demod->level_limit = DEFAULT_LEVEL_LIMIT;

	device_count = rtlsdr_get_device_count();
	if (!device_count) {
		fprintf(stderr, "No supported devices found.\n");
		exit(1);
	}

	fprintf(stderr, "Found %d device(s):\n", device_count);
	for (i = 0; i < device_count; i++) {
		rtlsdr_get_device_usb_strings(i, vendor, product, serial);
		fprintf(stderr, "  %d:  %s, %s, SN: %s\n", i, vendor, product, serial);
	}
	fprintf(stderr, "\n");

	fprintf(stderr, "Using device %d: %s\n",
		dev_index, rtlsdr_get_device_name(dev_index));

	r = rtlsdr_open(&dev, dev_index);
	if (r < 0) {
		fprintf(stderr, "Failed to open rtlsdr device #%d.\n", dev_index);

		exit(1);
	}

	sigact.sa_handler = sighandler;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;
	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGTERM, &sigact, NULL);
	sigaction(SIGQUIT, &sigact, NULL);
	sigaction(SIGPIPE, &sigact, NULL);

	/* Set the sample rate */
	r = rtlsdr_set_sample_rate(dev, samp_rate);
	if (r < 0)
		fprintf(stderr, "WARNING: Failed to set sample rate.\n");
	else
		fprintf(stderr, "Sample rate set to %d.\n", rtlsdr_get_sample_rate(dev)); // Unfortunately, doesn't return real rate

	/* Set the tuner gain */
	r = rtlsdr_set_tuner_gain_mode(dev, 0);
	if (r < 0)
		fprintf(stderr, "WARNING: Failed to enable automatic gain.\n");
	else
		fprintf(stderr, "Tuner gain set to Auto.\n");

	r = rtlsdr_set_freq_correction(dev, 0);

	/* Reset endpoint before we start reading from it (mandatory) */
	r = rtlsdr_reset_buffer(dev);
	if (r < 0)
		fprintf(stderr, "WARNING: Failed to reset buffers.\n");

	while (!do_exit) {
		/* Set the frequency */
		r = rtlsdr_set_center_freq(dev, freq);
		if (r < 0)
			fprintf(stderr, "WARNING: Failed to set center freq.\n");
		else
			fprintf(stderr, "Tuned to %u Hz.\n", rtlsdr_get_center_freq(dev));
		r = rtlsdr_read_async(dev, rtlsdr_callback, (void *)demod,
			0, out_block_size);
		do_exit_async = 0;
	}

	if (do_exit)
		fprintf(stderr, "\nUser cancel, exiting...\n");
	else
		fprintf(stderr, "\nLibrary error %d, exiting...\n", r);

	rtlsdr_close(dev);


	return 0;
}