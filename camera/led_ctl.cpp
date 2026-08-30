#include "led_ctl.h"

led_ctl::led_ctl()
{
    led_tri_fd=open(LED_TRIGGER,O_RDWR);
    if (0 > led_tri_fd) {
     perror("open error");
     exit(-1);
     }
    led_bri_fd=open(LED_BRIGHTNESS,O_RDWR);
    if (0 > led_bri_fd) {
     perror("open error");
     exit(-1);
     }
    write(led_tri_fd, "none", 4); //先将触发模式设置为 none
    write(led_bri_fd, "0", 1);
}

void led_ctl::led_on()
{
    write(led_tri_fd, "none", 4); //先将触发模式设置为 none
    write(led_bri_fd, "1", 1);
}
void led_ctl::led_off()
{
    write(led_tri_fd, "none", 4); //先将触发模式设置为 none
    write(led_bri_fd, "0", 1);
}

