#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <curl/curl.h>
#include "Logging.h"
#include "PowerPi.h"

int send_to_emon(const char * pname, int value)
{
	CURL *curl;
	CURLcode res;
	
	if (strlen(EmonKey) != 32)
		return -1;

	char url[256];

	if (TimeFromPi)
	{
		sprintf(url, "%s?json={%s:{%u}}&apikey=%s&time=%u", "http://emoncms.org/input/post.json", pname, value, EmonKey, time(NULL));
	}
	else
	{
		sprintf(url, "%s?json={%s:{%u}}&apikey=%s", "http://emoncms.org/input/post.json", pname, value, EmonKey);
	}
	

	curl = curl_easy_init();
	if (curl) {
		curl_easy_setopt(curl, CURLOPT_URL, url);

		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);

		res = curl_easy_perform(curl);

		if (res != CURLE_OK)
		{
			/*char err[256];
			sprintf(err, "curl_easy_perform() failed: %s\n",
				curl_easy_strerror(res));*/
			//log_string_to_file(err);
		}

		curl_easy_cleanup(curl);
	}

	return res;
}