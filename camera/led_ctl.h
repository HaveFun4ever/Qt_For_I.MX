#ifndef LED_CTL_H
#define LED_CTL_H
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#define LED_TRIGGER "/sys/class/leds/sys-led/trigger"
#define LED_BRIGHTNESS "/sys/class/leds/sys-led/brightness"

class led_ctl
{
public:
    int led_tri_fd;
    int led_bri_fd;
    led_ctl();
    void led_on();
    void led_off();
};

#endif // LED_CTL_H
