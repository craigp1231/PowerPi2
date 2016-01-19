#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <curl/curl.h>
#include "Logging.h"
#include "PowerPi.h"
#include <pthread.h>


time_t SEND_TIME = 0;
struct DEVICE_VALUE DEVICE_BUFFER[20];
unsigned int DEVICE_COUNT = 0;

void submitURL(const char *url)
{
	CURL *curl;
 
	curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
	curl_easy_perform(curl);
	curl_easy_cleanup(curl);
}

static void *process_buffer(void* dCnt)
{
	char url[256];
	char data[256];
	memset(data, 0, 255);
	for (int i = 0; i < DEVICE_COUNT; i++)
	{
		sprintf(data + strlen(data), ",device_%03X:{%u}", DEVICE_BUFFER[i].device_id, DEVICE_BUFFER[i].device_value);
	}
	
	sprintf(url, "%s?json={%s}&apikey=%s", "http://emoncms.org/input/post.json", data+1, EmonKey);
	
	if (Verbose)
		fprintf(stderr, "URL: %s\n", url);
	
	submitURL(url);
	
	DEVICE_COUNT = 0;
	memset(DEVICE_BUFFER, 0, 19 * 4);
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
	if (SEND_TIME == 0)
	{
		curl_global_init(CURL_GLOBAL_ALL);
		SEND_TIME = time(NULL);
	}		
	
	// Add the value to buffer
	int idx = search_for_address(address);
	DEVICE_BUFFER[idx].device_id = address;
	DEVICE_BUFFER[idx].device_value = value;
	DEVICE_COUNT++;
	
	// If the time buffer has elapsed (send to emon)
	if (time(NULL) - SEND_TIME >= TimeBuffer)
	{
		pthread_t tid[1];
		pthread_create(&tid[0],
			NULL,
			process_buffer,
			(void*)&DEVICE_COUNT);
		SEND_TIME = time(NULL);
		
	}
	
}