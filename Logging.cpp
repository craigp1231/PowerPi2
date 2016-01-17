#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <curl/curl.h>
#include "Logging.h"
#include "PowerPi.h"
#include <pthread.h>

static void *submitURL(void *url)
{
	CURL *curl;
 
	curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_perform(curl);
	curl_easy_cleanup(curl);
 
	return NULL;
}

void send_to_emon(const char * pname, int value)
{	
	pthread_t tid[1];
		
	if (strlen(EmonKey) != 32)
		return;

	char url[256];

	if (TimeFromPi)
	{
		sprintf(url, "%s?json={%s:{%u}}&apikey=%s&time=%u", "http://emoncms.org/input/post.json", pname, value, EmonKey, time(NULL));
	}
	else
	{
		sprintf(url, "%s?json={%s:{%u}}&apikey=%s", "http://emoncms.org/input/post.json", pname, value, EmonKey);
	}
	
	curl_global_init(CURL_GLOBAL_ALL);
	
	pthread_create(&tid[0],
		NULL,
		submitURL,
		(void *)url);
}