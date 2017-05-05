#ifndef DHT_H_
#define DHT_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <osapi.h>
#include "common/platforms/esp8266/esp_missing_includes.h"
#include "fw/src/mgos_gpio.h"
#include "fw/src/mgos_hal.h"

typedef struct {
	float humidity;
	float temp;
	char* err;
} dht_data;

inline unsigned int readBit(int pin, int* max) {
	int high = *max;
	while((*max)-- > 0 && mgos_gpio_read(pin) == true) ;
	high = high - *max;

	int low = *max;
	while((*max)-- > 0 && mgos_gpio_read(pin) == false) ;
	low = low - *max;

	return high > low;
}

inline unsigned int readByte(int pin, int* max) {
	int byte = 0, bit = 8;
	while(bit-- > 0) {
		byte = byte << 1 | readBit(pin, max);
	}

	return byte;
}

static dht_data* Read_DHT(int pin, bool farenheit) {
	int data[5] = { 0, 0, 0, 0, 0 };
	int max = 100000;
	dht_data *res = (dht_data*)malloc(sizeof(dht_data) + 65); // max err string is 64 chars
	memset(res, 0, sizeof(dht_data) + 65);
	assert(res != 0);
 
	mgos_gpio_set_mode(pin, MGOS_GPIO_MODE_OUTPUT);
	mgos_gpio_set_pull(pin, MGOS_GPIO_PULL_NONE);
	// clear any activity by pulling high for 250ms
	// high is the line free signal
	mgos_gpio_write(pin, true);
	os_delay_us(250 * 1000);

	// send start signal with a pull low for 20ms
	mgos_gpio_write(pin, false);
	os_delay_us(20 * 1000);

	// timing critical - no interrupts
	mgos_ints_disable();

	// The DHT will pull low--input with a pullup.
	mgos_gpio_write(pin, true);
	os_delay_us(40);
	mgos_gpio_set_mode(pin, MGOS_GPIO_MODE_INPUT);
	mgos_gpio_set_pull(pin, MGOS_GPIO_PULL_UP);
	os_delay_us(10);

	readBit(pin, &max);
	readBit(pin, &max);

	if (max <= 0) {
		strcpy(res->err, "start-up timeout");
		mgos_ints_enable();
		return res;
	}

	// Read 5 bytes.
	data[0] = readByte(pin, &max);
	data[1] = readByte(pin, &max);
	data[2] = readByte(pin, &max);
	data[3] = readByte(pin, &max);
	data[4] = readByte(pin, &max);
	
	// re-enable interrupts
	mgos_ints_enable();

	if (max <= 0) {
		strcpy(res->err, "max time exceeded in reads");
		return res;
	}

	// verify checksum.
	if (data[4] != ((data[0] + data[1] + data[2] + data[3]) & 0xFF)) {
		strcpy(res->err, "checksum failed");
		return res;
	}

	res->humidity = (float)((data[0] << 8) + data[1]) / 10;
	if ( res->humidity > 100 )
		res->humidity = data[0]; // for DHT11

	// res->temp = (float)(((data[2] & 0x7F) << 8) + data[3]) / 10;
	res->temp = (float)(((data[2]) << 8) + data[3]) / 10;
	if ( res->temp > 125 )
		res->temp = data[2]; // for DHT11

	//if ( data[2] & 0x80 )
	//	res->temp = -res->temp;

	if (farenheit)
		res->temp = res->temp * 1.8f + 32;

	return res;
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DHT_H_ */
