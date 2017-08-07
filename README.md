# ESP8266 WS2812 RGB LED Driver in C
The purpose of this repository is to provide a stand-alone routine in C to drive WS2812 (Neopixel) RGB LEDs from an ESP8266. The routine, `ws2812_write()`, is in `user_main.c`. (Also necessary is the small `_getCycleCount()` routine.) These routines can be copied into an existing project file or can be placed in a new project file.

My goal was to address the needs of developers, like myself, who want to get something working quickly and who are not currently using a larger environment on the ESP8266, such as Arduino or NodeMCU.

## Example Usage

```c
#define WS2812_LED_GPIO 2

static void ICACHE_FLASH_ATTR update_leds(void)
{
	uint8_t colors[21] = {0, 0, 0, 32, 0, 0,   0, 32, 0,   0, 0, 32,   32, 32, 0,  32, 0, 32,  0, 32, 32 };
	
	ws2812_write(WS2812_LED_GPIO, colors, 21);
}
```

## Notes
Note that the color sequence is Green-Red-Blue (GRB) and not the traditional Red-Green-Blue (RGB)

## Credits
The `ws2812_write()` routine is copied from @kbeckmann's [NodeMCU repository](https://github.com/kbeckmann/nodemcu-firmware). For more information see the related post [Bit-banging two pins simultaneously on ESP8266](https://kbeckmann.github.io/2015/07/25/Bit-banging-two-pins-simultaneously-on-ESP8266/)

The `Makefile` is copied from [Tuan PM's MQTT repository](https://github.com/tuanpmt/esp_mqtt). I flattened the directory structure and made some minor changes to the Makefile.

## References
See also 
* @adafruit's [Neopixel Library](https://github.com/adafruit/Adafruit_NeoPixel/blob/master/esp8266.c): Handles the ESP8266 and the ESP32
* @cnlohr's [original ws2812 project](https://github.com/cnlohr/ws2812esp8266)
* @cnlohr's [I2C approach to driving ws2812](https://github.com/cnlohr/esp8266ws2812i2s): Reduces the CPU load by offloading the waveform creation to the I2C core
* @kbeckmann's [Single pin and two pin bit-banging](https://github.com/kbeckmann/nodemcu-firmware/blob/ws2812-dual/app/modules/ws2812.c): Provides a version of ws2812_write() that simultaneously updates the LEDs on two GPIO pins.
* [Scargill's Tech Blog](https://tech.scargill.net/ws2812b-success-on-the-esp-12/)



## GPIO Set-up
The ESP8266 pin must be configured as a GPIO pin. 

For pin 2, for example, these lines are necessary:

```c
    // Set pin mux for Pin 2 to "GPIO Alternate Function"
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2); 
    
    // Enable pin 2
    GPIO_REG_WRITE(GPIO_ENABLE_W1TS_ADDRESS, (1<<2));
```

## Routines (for copy-and-paste)
If you just want to get started and are not interested in the comments, then you can copy and paste these two routines:

```c
static inline uint32_t _getCycleCount(void)
{
	uint32_t cycles;
	__asm__ __volatile__("rsr %0,ccount":"=a" (cycles));
	return cycles;
}

static void ws2812_write(uint8_t pin, uint8_t *pixels, uint32_t length) 
{
	uint8_t *p, *end, pixel, mask;
	uint32_t t, t0h, t1h, ttot, c, start_time, pin_mask;

	pin_mask = 1 << pin;
	p =  pixels;
	end =  p + length;
	pixel = *p++;
	mask = 0x80;
	start_time = 0;
	t0h  = (1000 * system_get_cpu_freq()) / 3333;  // 0.30us (spec=0.35 +- 0.15)
	t1h  = (1000 * system_get_cpu_freq()) / 1666;  // 0.60us (spec=0.70 +- 0.15)
	ttot = (1000 * system_get_cpu_freq()) /  800;  // 1.25us (MUST be >= 1.25)

	ets_intr_lock();

	while (true)
	{
		t = (pixel & mask) ? t1h : t0h;

		while (((c = _getCycleCount()) - start_time) < ttot); // Wait for the previous bit to finish
		GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, pin_mask);      // Set pin high
		
		start_time = c;                                       // Save the start time
		while (((c = _getCycleCount()) - start_time) < t);    // Wait for high time to finish
		GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, pin_mask);      // Set pin low
		
		if (!(mask >>= 1))                 					  // Next bit/byte
		{                 
			if (p >= end)
				break;

			pixel= *p++;
			mask = 0x80;
		}
	}
	
	ets_intr_unlock();
}
```
