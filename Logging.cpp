#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <curl/curl.h>
#include "Logging.h"
#include "PowerPi.h"
#include <pthread.h>


bool hasInit;
struct DEVICE_VALUE DEVICE_BUFFER[20];
unsigned int DEVICE_COUNT = 0;
pthread_t thread_id;

CURL *curl;

void sleep_milliseconds(uint32_t millis) {
	struct timespec sleep;
	sleep.tv_sec = millis / 1000;
	sleep.tv_nsec = (millis % 1000) * 1000000L;
	while (clock_nanosleep(CLOCK_MONOTONIC, 0, &sleep, &sleep) && errno == EINTR)
		;
}

void submitURL(const char *url)
{
	curl_easy_reset(curl);
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_perform(curl);
	//curl_easy_cleanup(curl);
}

void process_buffer()
{
	char url[256];
	char data[256];
	memset(data, 0, 255);
	for (int i = 0; i < DEVICE_COUNT; i++)
	{
		sprintf(data + strlen(data), ",device_%03X:{%u}", DEVICE_BUFFER[i].device_id, DEVICE_BUFFER[i].device_value);
	}
	
	sprintf(url, "%s?json={%s}&apikey=%s", "http://emoncms.org/input/post.json", data + 1, EmonKey);
	
	if (Verbose)
		fprintf(stderr, "URL: %s\n", url);
	
	submitURL(url);
	
	DEVICE_COUNT = 0;
	memset(DEVICE_BUFFER, 0, 19 * 4);
}

static void *thread_handler(void* p)
{
	while (1)
	{
		sleep_milliseconds(TimeBuffer * 1000);			// Wait for data
		process_buffer();								// Process data
	}
}

unsigned int search_for_address(unsigned int address)
{
	for (int i = 0; i < DEVICE_COUNT; i++)
	{
		if (DEVICE_BUFFER[DEVICE_COUNT].device_id == address)
			return i;
	}
	return DEVICE_COUNT;
}

void send_to_emon(unsigned short address, unsigned short value)
{
	// No point doing anything if we dont have an Emon Key
	if (strlen(EmonKey) != 32)
		return;
	
	// Init send time
	if (!hasInit)
	{
		curl_global_init(CURL_GLOBAL_ALL);
		
		curl = curl_easy_init();
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);						// Timeout in seconds
		curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);					// If a timeout does occur, dont signal to the app to close
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 0);						// Dont display output
		curl_easy_setopt(curl, CURLOPT_DNS_CACHE_TIMEOUT, 86400);		// 1 day (so it doesnt do a DNS request each minute, saves time)
		
		int err = pthread_create(&thread_id,
			NULL,
			thread_handler,
			NULL);
		if (err && Verbose)
		{
			fprintf(stderr, "Thread Error: %u\n", err);
		}
		
		hasInit = true;
	}		
	
	// Add the value to buffer
	int idx = search_for_address(address);
	DEVICE_BUFFER[idx].device_id = address;
	DEVICE_BUFFER[idx].device_value = value;
	DEVICE_COUNT++;	
}