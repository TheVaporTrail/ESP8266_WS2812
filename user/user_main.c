/*--------------------------------------------------------------------------------------
 * user_main.c
 *
 *--------------------------------------------------------------------------------------*/
#include "ets_sys.h"
#include "uart.h"
#include "osapi.h"
#include "gpio.h"
#include "user_interface.h"

/*--------------------------------------------------------------------------------------
 * Debug Printf and Target Name
 *   For debug builds, map dbg_printf to os_printf, other to nothing
 *   Define TARGET_NAME if it was not defined by the Makefile.
 *--------------------------------------------------------------------------------------*/
#if defined(DEBUG_ON)
	#define dbg_printf(format, ...) os_printf(format, ## __VA_ARGS__)
#else
	#define dbg_printf(format, ...)
#endif

#if !defined(TARGET_NAME)
	#define TARGET_NAME "ws2812"
#endif

/*--------------------------------------------------------------------------------------
 * Flush UART Attempt
 *   On my system there is a lot of noise from the ESP8266 boot code. This routine
 *   attempts to flush the UART so the first printf will not get lost. This does
 *   not always work.
 *--------------------------------------------------------------------------------------*/
static void ICACHE_FLASH_ATTR flush_uart_attempt(void)
{
	#if defined(DEBUG_ON)
	int j;
	
	for (j = 0; j < 20; j++)
	{
		os_delay_us(1000); // 1ms
		dbg_printf(".\r\n");
	}
	#endif
}

/*--------------------------------------------------------------------------------------
 * Report Project Info
 *--------------------------------------------------------------------------------------*/
static void ICACHE_FLASH_ATTR report_project_info(void)
{
	dbg_printf("[%s] -------------------------------------------\n", TARGET_NAME);
	dbg_printf("[%s] Minimal ESP8266 WS2812 RGB LED application\r\n", TARGET_NAME);
	dbg_printf("[%s] -------------------------------------------\n", TARGET_NAME);
	dbg_printf("[%s] SDK: %s\r\n", TARGET_NAME, system_get_sdk_version());
	dbg_printf("[%s] Chip ID: %08X\r\n", TARGET_NAME, system_get_chip_id());
	dbg_printf("[%s] CPU Freq: %d\r\n", TARGET_NAME, system_get_cpu_freq());
	dbg_printf("[%s] Memory info:\r\n", TARGET_NAME);
	system_print_meminfo();

	dbg_printf("[%s] -------------------------------------------\n", TARGET_NAME);
	dbg_printf("[%s] Build time: %s\n", TARGET_NAME, BUID_TIME);
	dbg_printf("[%s] -------------------------------------------\n", TARGET_NAME);
}

/*--------------------------------------------------------------------------------------
 * Get Cycle Count
 *   This is needed by ws2812_write to generate the correct timing
 *--------------------------------------------------------------------------------------*/
static inline uint32_t _getCycleCount(void)
{
	uint32_t cycles;
	__asm__ __volatile__("rsr %0,ccount":"=a" (cycles));
	return cycles;
}

/*--------------------------------------------------------------------------------------
 * WS2812 Write
 *   Write a buffer of GRB values to the specified pin in the WS2812 protocol format
 *   Note that the sequence is Green-Red-Blue and not the traditional Red-Green-Blue
 *--------------------------------------------------------------------------------------*/
// https://github.com/kbeckmann/nodemcu-firmware/blob/ws2812-dual/app/modules/ws2812.c
//
// This algorithm reads the cpu clock cycles to calculate the correct
// pulse widths. It works in both 80 and 160 MHz mode.
// The values for t0h, t1h, ttot have been tweaked and it doesn't get faster than this.
// The datasheet is confusing and one might think that a shorter pulse time can be achieved.
// The period has to be at least 1.25us, even if the datasheet says:
//   T0H: 0.35 (+- 0.15) + T0L: 0.8 (+- 0.15), which is 0.85<->1.45 us.
//   T1H: 0.70 (+- 0.15) + T1L: 0.6 (+- 0.15), which is 1.00<->1.60 us.
// Anything lower than 1.25us will glitch in the long run.
//
// DDK 2017.08.02: Removed ICACHE_RAM_ATTR as it was causing compile error
//                 Added ets_intr_lock/_unlock into this routine, instead of the caller
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

/*--------------------------------------------------------------------------------------
 * Update LEDs
 *   Set the LEDs to this sequence of dim (not very bright) colors:
 *     off, green, red, blue, yellow, cyan, magenta
 *--------------------------------------------------------------------------------------*/
#define WS2812_LED_GPIO 2

static void ICACHE_FLASH_ATTR update_leds(void)
{
	uint8_t colors[21] = {0, 0, 0, 32, 0, 0,   0, 32, 0,   0, 0, 32,   32, 32, 0,  32, 0, 32,  0, 32, 32 };
	
	ws2812_write(WS2812_LED_GPIO, colors, 21);
}

/*--------------------------------------------------------------------------------------
 * LED GPIO Pin 2 Init
 *--------------------------------------------------------------------------------------*/
void ICACHE_FLASH_ATTR led_gpio_pin2_init(void) 
{
    // Set pin mux for Pin 2 to "GPIO Alternate Function"
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2); 
    
    // Enable pin 2
    GPIO_REG_WRITE(GPIO_ENABLE_W1TS_ADDRESS, (1<<2));
}

/*--------------------------------------------------------------------------------------
 * App Init
 *--------------------------------------------------------------------------------------*/
static void ICACHE_FLASH_ATTR app_init(void)
{
	// Initialize the UART, attempt to flush it, and report the project info
	uart_init(BIT_RATE_115200, BIT_RATE_115200);
	flush_uart_attempt();
	report_project_info();

	// Initialize Pin 2 as GPIO
	// Note that this is only for Pin 2. It would need to be different
	// if the WS2812 LEDs are on another pin.
	led_gpio_pin2_init();
	
	// Delay for gpio pin2 to settle. The 30ms value is arbitrary.
	// I added this to address the issue that the first LED was not set
	// correctly when I only updated the LEDs once and from this routine.
	// If you update the LEDs later and/or with a timer, then this
	// delay is probably not necessary
	os_delay_us(30 * 1000); // 30ms
	
    // Update the LEDs to a fixed set of colors
    update_leds();

	dbg_printf("[%s] User code execution has completed.\n\r", TARGET_NAME);
}

/*--------------------------------------------------------------------------------------
 * User Init
 *--------------------------------------------------------------------------------------*/
void user_init(void)
{	
	system_init_done_cb(app_init);
}
