#ifndef INCLUDE_LOGGING_H_
#define INCLUDE_LOGGING_H_

struct DEVICE_VALUE
{
	unsigned short device_id;
	unsigned short device_value;
};

void send_to_emon(unsigned short address, unsigned short value);

#endif