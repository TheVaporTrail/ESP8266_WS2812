# ESP8266 WS2812 RGB LED Driver in C
The purpose of this repository is to provide a routine in C to drive WS2812 (Neopixel) RGB LEDs from an ESP8266. 

## Credits
The ws2812_write() routine is copied from [kbeckmann's NodeMCU repository](https://github.com/kbeckmann/nodemcu-firmware/blob/ws2812-dual/app/modules/ws2812.c). For more information I recommended reading the related post (Bit-banging two pins simultaneously on ESP8266)[https://kbeckmann.github.io/2015/07/25/Bit-banging-two-pins-simultaneously-on-ESP8266/]

The Makefile is copied from (Tuan PM's MQTT repository)[https://github.com/tuanpmt/esp_mqtt]. I flattened the directory structure and made some minor changes to the Makefile.
