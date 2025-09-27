#ifndef DEVICE_H_
#define DEVICE_H_

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <time.h>
#include <stdarg.h>
#include <sys/ioctl.h>
#include <linux/serial.h>

#define IN 0
#define OUT 1

#define LOW 0
#define HIGH 1

//  GPIO handler structure to manage requests typedef struct
typedef struct
{
    struct gpiod_line_request *request;
    unsigned int offset;
} gpio_handler_t;

struct gpiod_line_request *create_line_request(unsigned int offset, int direction, int initial_value, const char *consumer);

gpio_handler_t *gpio_setup_output(unsigned int pin, int initial_value);
gpio_handler_t *gpio_setup_input(unsigned int pin);
int gpio_write(gpio_handler_t *handler, int value);
int gpio_read(gpio_handler_t *handler);

int initDevice(char *devicePath, int senseLineType, int senseLinePin);
int closeDevice();
int readBytes(unsigned char *buffer, int amount);
int writeBytes(unsigned char *buffer, int amount);
int setSenseLine(int state);

#endif // DEVICE_H_
