#include "hardware/device.h"
#include "console/debug.h"

#include <gpiod.h>

#define TIMEOUT_SELECT 200

int serialIO = -1;
int localSenseLinePin = 12;
int localSenseLineType = 0;

int setSerialAttributes(int fd, int myBaud);
int setupGPIO(int pin);
int setGPIODirection(int pin, int dir);
int writeGPIO(int pin, int value);

struct gpiod_chip *chip;
struct gpiod_line *line;

int initDevice(char *devicePath, int senseLineType, int senseLinePin)
{
  if ((serialIO = open(devicePath, O_RDWR | O_NOCTTY | O_SYNC | O_NDELAY)) < 0)
    return 0;

  /* Setup the serial connection */
  setSerialAttributes(serialIO, B115200);

  /* Copy variables over from config */
  localSenseLineType = senseLineType;
  localSenseLinePin = senseLinePin;

  /* Setup the GPIO pins */
  if (localSenseLineType && setupGPIO(localSenseLinePin) == -1)
    debug(0, "Sense line pin %d not available\n", senseLinePin);

  /* Setup the GPIO pins initial state */
  switch (senseLineType)
  {
  case 0:
    debug(1, "Debug: No sense line set\n");
    break;
  case 1:
    debug(1, "Debug: Float/Sync sense line set\n");
    setGPIODirection(senseLinePin, IN);
    break;
  case 2:
    debug(1, "Debug: Complex sense line set\n");
    setGPIODirection(senseLinePin, OUT);
    break;
  default:
    debug(0, "Debug: Invalid sense line type set\n");
    break;
  }

  /* Initially float the sense line */
  setSenseLine(0);

  return 1;
}

int closeDevice()
{
  tcflush(serialIO, TCIOFLUSH);
  return close(serialIO) == 0;
}

int readBytes(unsigned char *buffer, int amount)
{
  fd_set fd_serial;
  struct timeval tv;

  FD_ZERO(&fd_serial);
  FD_SET(serialIO, &fd_serial);

  tv.tv_sec = 0;
  tv.tv_usec = TIMEOUT_SELECT * 1000;

  int filesReadyToRead = select(serialIO + 1, &fd_serial, NULL, NULL, &tv);

  if (filesReadyToRead < 1)
    return -1;

  if (!FD_ISSET(serialIO, &fd_serial))
    return -1;

  return read(serialIO, buffer, amount);
}

int writeBytes(unsigned char *buffer, int amount)
{
  return write(serialIO, buffer, amount);
}

/* Sets the configuration of the serial port */
int setSerialAttributes(int fd, int myBaud)
{
  struct termios options;
  int status;
  tcgetattr(fd, &options);

  cfmakeraw(&options);
  cfsetispeed(&options, myBaud);
  cfsetospeed(&options, myBaud);

  options.c_cflag |= (CLOCAL | CREAD);
  options.c_cflag &= ~PARENB;
  options.c_cflag &= ~CSTOPB;
  options.c_cflag &= ~CSIZE;
  options.c_cflag |= CS8;
  options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
  options.c_oflag &= ~OPOST;

  options.c_cc[VMIN] = 0;
  options.c_cc[VTIME] = 0; // One seconds (10 deciseconds)

  tcsetattr(fd, TCSANOW, &options);

  ioctl(fd, TIOCMGET, &status);

  status |= TIOCM_DTR;
  status |= TIOCM_RTS;

  ioctl(fd, TIOCMSET, &status);

  usleep(100 * 1000); // 10mS

  struct serial_struct serial_settings;

  ioctl(fd, TIOCGSERIAL, &serial_settings);

  serial_settings.flags |= ASYNC_LOW_LATENCY;
  ioctl(fd, TIOCSSERIAL, &serial_settings);

  tcflush(serialIO, TCIOFLUSH);
  usleep(100 * 1000); // Required to make flush work, for some reason

  return 0;
}

int setupGPIO(int pin)
{
  struct gpiod_chip *chip = gpiod_chip_open("/dev/gpiochip0");
  if (!chip)
  {
    debug(1, "Error: cannot open chip /dev/gpiochip0");
    return 0;
  }

  struct gpiod_request_config *req_cfg = gpiod_request_config_new();
  if (!req_cfg)
  {
    debug(1, "Error: cannot create request config");
    gpiod_chip_close(chip);
    return 0;
  }
  gpiod_request_config_set_consumer(req_cfg, "openjvs-sense");

  struct gpiod_line_settings *line_settings = gpiod_line_settings_new();
  if (!line_settings)
  {
    debug(1, "Error: cannot create line settings");
    gpiod_request_config_free(req_cfg);
    gpiod_chip_close(chip);
    return 0;
  }
  // Default to input, can be changed later by setGPIODirection
  gpiod_line_settings_set_direction(line_settings, GPIOD_LINE_DIRECTION_INPUT);

  struct gpiod_line_config *line_config = gpiod_line_config_new();
  if (!line_config)
  {
    debug(1, "Error: cannot create line config");
    gpiod_line_settings_free(line_settings);
    gpiod_request_config_free(req_cfg);
    gpiod_chip_close(chip);
    return 0;
  }

  unsigned int LINE_OFFSET = (unsigned int)pin;
  int rc = gpiod_line_config_add_line_settings(line_config, &LINE_OFFSET, 1, line_settings);
  if (rc < 0)
  {
    debug(1, "Error: cannot add line settings to line config");
    gpiod_line_config_free(line_config);
    gpiod_line_settings_free(line_settings);
    gpiod_request_config_free(req_cfg);
    gpiod_chip_close(chip);
    return 0;
  }

  struct gpiod_line_request *request = gpiod_chip_request_lines(chip, req_cfg, line_config);
  if (!request)
  {
    debug(1, "Error: cannot request line for setup");
    rc = 0;
  }
  else
  {
    debug(1, "Debug: GPIO pin %d setup complete\n", pin);
    gpiod_line_request_release(request);
    rc = 1;
  }

  gpiod_line_config_free(line_config);
  gpiod_line_settings_free(line_settings);
  gpiod_request_config_free(req_cfg);
  gpiod_chip_close(chip);

  return rc;
}

int setGPIODirection(int pin, int dir)
{
  int rc = 0;
  struct gpiod_chip *chip = gpiod_chip_open("/dev/gpiochip0");
  if (chip == NULL)
  {
    debug(1, "Error: cannot open chip /dev/gpiochip0");
    return 0;
  }

  struct gpiod_request_config *req_cfg = gpiod_request_config_new();
  if (req_cfg == NULL)
  {
    debug(1, "Error: cannot create request config");
    gpiod_chip_close(chip);
    return 0;
  }
  gpiod_request_config_set_consumer(req_cfg, "openjvs-sense");

  struct gpiod_line_settings *line_settings = gpiod_line_settings_new();
  if (line_settings == NULL)
  {
    debug(1, "Error: cannot create line settings");
    gpiod_request_config_free(req_cfg);
    gpiod_chip_close(chip);
    return 0;
  }

  if (dir == IN)
    gpiod_line_settings_set_direction(line_settings, GPIOD_LINE_DIRECTION_INPUT);
  else
    gpiod_line_settings_set_direction(line_settings, GPIOD_LINE_DIRECTION_OUTPUT);

  struct gpiod_line_config *line_config = gpiod_line_config_new();
  if (line_config == NULL)
  {
    debug(1, "Error: cannot create line config");
    gpiod_line_settings_free(line_settings);
    gpiod_request_config_free(req_cfg);
    gpiod_chip_close(chip);
    return 0;
  }

  unsigned int LINE_OFFSET = (unsigned int)pin;
  rc = gpiod_line_config_add_line_settings(line_config, &LINE_OFFSET, 1, line_settings);
  if (rc < 0)
  {
    debug(1, "Error: cannot add line settings to line config");
    rc = 0;
  }
  else
  {
    struct gpiod_line_request *request = gpiod_chip_request_lines(chip, req_cfg, line_config);
    if (request == NULL)
    {
      debug(1, "Error: cannot request line for direction");
      rc = 0;
    }
    else
    {
      debug(1, "Debug: GPIO pin %d direction set to %s\n", pin, dir == IN ? "IN" : "OUT");
      gpiod_line_request_release(request);
      rc = 1;
    }
  }

  gpiod_line_config_free(line_config);
  gpiod_line_settings_free(line_settings);
  gpiod_request_config_free(req_cfg);
  gpiod_chip_close(chip);

  return rc;
}

// 2 major changes here for Raspberry PI 5 support with latest builds:
// - use libgpiod instead of sysfs (deprecated)
// - change config.txt to use dtoverlay=uart2-pi5 (to keep GPIOs 4-5 for openJVS HAT with all jumpers on the left)
// - set openjvs config to DEVICE_PATH /dev/ttyAMA2
//
// Some doc:
//   changes in dtoverlay:   https://github.com/raspberrypi/firmware/blob/master/boot/overlays/README
//   GPIO's:                 https://pip.raspberrypi.com/categories/685-app-notes-guides-whitepapers/documents/RP-006553-WP/A-history-of-GPIO-usage-on-Raspberry-Pi-devices-and-current-best-practices.pdf
//   openJVS HAT GPIO usage: https://github.com/OpenJVS/OpenJVS/blob/master/docs/OpenJVS_IO_Manual_1.2.pdf
//   new tools:              https://libgpiod.readthedocs.io/en/latest/gpio_tools.html
int writeGPIO(int pin, int value)
{
  int rc = 0;
  chip = gpiod_chip_open("/dev/gpiochip0");
  if (chip == NULL)
  {
    debug(1, "Error: can not open chip /dev/gpiochip0");
  }
  else
  {
    // CREATE A NEW REQUEST CONFIG
    struct gpiod_request_config *req_cfg = gpiod_request_config_new();
    if (req_cfg == NULL)
    {
      debug(1, "Error: can not create request config");
    }
    else
    {
      // SET REQUEST CONFIG NAME
      gpiod_request_config_set_consumer(req_cfg, "openjvs-sense");

      // CREATE A NEW LINE SETTINGS
      struct gpiod_line_settings *line_settings = gpiod_line_settings_new();
      if (line_settings == NULL)
      {
        debug(1, "Error: can not create line settings");
      }
      else
      {
        // SET LINE DIRECTION AND VALUE
        gpiod_line_settings_set_direction(line_settings, GPIOD_LINE_DIRECTION_OUTPUT);
        gpiod_line_settings_set_output_value(line_settings, value ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE);

        // LINE CONFIG
        struct gpiod_line_config *line_config = gpiod_line_config_new();

        if (line_config == NULL)
        {
          debug(1, "Error: can not create line config");
        }
        else
        {
          // ADD LINE SETTINGS
          unsigned int LINE_OFFSET = (unsigned int)pin;
          rc = gpiod_line_config_add_line_settings(line_config, &LINE_OFFSET, 1, line_settings);
          if (rc < 0)
          {
            debug(1, "Error: can not add line settings to line config");
          }
          else
          {
            // SET OUTPUT VALUE
            enum gpiod_line_value output_value = value ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE;
            rc = gpiod_line_config_set_output_values(line_config, &output_value, 1);
            if (rc < 0)
            {
              debug(1, "Error: can not set output value");
            }
            else
            {
              struct gpiod_line_request *request = gpiod_chip_request_lines(chip, req_cfg, line_config);
              if (request == NULL)
              {
                debug(1, "Error: can not request line");
                rc = 0;
              }
              else
              {
                // Success
                debug(1, "Debug: GPIO pin %d set to %d\n", pin, value);
                gpiod_line_request_release(request);
                rc = 1;
              }
            }
          }
          gpiod_line_settings_free(line_settings);
          gpiod_line_config_free(line_config);
        }
        gpiod_request_config_free(req_cfg);
      }
      gpiod_chip_close(chip);
    }
  }

  return rc;
}

int readGPIO(int pin)
{
  int value = -1;
  struct gpiod_chip *chip = gpiod_chip_open("/dev/gpiochip0");
  if (chip == NULL)
  {
    debug(1, "Error: cannot open chip /dev/gpiochip0");
    return -1;
  }

  struct gpiod_request_config *req_cfg = gpiod_request_config_new();
  if (req_cfg == NULL)
  {
    debug(1, "Error: cannot create request config");
    gpiod_chip_close(chip);
    return -1;
  }
  gpiod_request_config_set_consumer(req_cfg, "openjvs-sense");

  struct gpiod_line_settings *line_settings = gpiod_line_settings_new();
  if (line_settings == NULL)
  {
    debug(1, "Error: cannot create line settings");
    gpiod_request_config_free(req_cfg);
    gpiod_chip_close(chip);
    return -1;
  }
  gpiod_line_settings_set_direction(line_settings, GPIOD_LINE_DIRECTION_INPUT);

  struct gpiod_line_config *line_config = gpiod_line_config_new();
  if (line_config == NULL)
  {
    debug(1, "Error: cannot create line config");
    gpiod_line_settings_free(line_settings);
    gpiod_request_config_free(req_cfg);
    gpiod_chip_close(chip);
    return -1;
  }

  unsigned int LINE_OFFSET = (unsigned int)pin;
  int rc = gpiod_line_config_add_line_settings(line_config, &LINE_OFFSET, 1, line_settings);
  if (rc < 0)
  {
    debug(1, "Error: cannot add line settings to line config");
    gpiod_line_config_free(line_config);
    gpiod_line_settings_free(line_settings);
    gpiod_request_config_free(req_cfg);
    gpiod_chip_close(chip);
    return -1;
  }

  struct gpiod_line_request *request = gpiod_chip_request_lines(chip, req_cfg, line_config);
  if (request == NULL)
  {
    debug(1, "Error: cannot request line for reading");
    rc = -1;
  }
  else
  {
    enum gpiod_line_value line_value;
    rc = gpiod_line_request_get_values(request, &line_value);
    if (rc < 0)
    {
      debug(1, "Error: cannot read value from line %d", pin);
      value = -1;
    }
    else
    {
      value = (line_value == GPIOD_LINE_VALUE_ACTIVE) ? 1 : 0;
    }
    gpiod_line_request_release(request);
  }

  gpiod_line_config_free(line_config);
  gpiod_line_settings_free(line_settings);
  gpiod_request_config_free(req_cfg);
  gpiod_chip_close(chip);
  return value;
}

int setSenseLine(int state)
{
  if (localSenseLineType == 0)
    return 1;

  switch (localSenseLineType)
  {
  /* Normal Float Style */
  case 1:
  {
    if (!state)
    {
      if (!setGPIODirection(localSenseLinePin, IN))
      {
        debug(1, "Warning: Failed to float sense line %d\n", localSenseLinePin);
        return 0;
      }
    }
    else
    {
      if (!setGPIODirection(localSenseLinePin, OUT) || !writeGPIO(localSenseLinePin, 0))
      {
        debug(1, "Warning: Failed to sink sense line %d\n", localSenseLinePin);
        return 0;
      }
    }
  }
  break;

  /* Switch Style */
  case 2:
  {
    if (!state)
    {
      if (!writeGPIO(localSenseLinePin, 0))
      {
        printf("Warning: Failed to set sense line to 1 %d\n", localSenseLinePin);
        return 0;
      }
    }
    else
    {
      if (!writeGPIO(localSenseLinePin, 1))
      {
        printf("Warning: Failed to sink sense line %d\n", localSenseLinePin);
        return 0;
      }
    }
  }
  break;

  default:
    debug(0, "Invalid sense line type set\n");
    break;
  }

  return 1;
}
