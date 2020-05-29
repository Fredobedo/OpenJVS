#include "input.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <linux/input.h>
#include <ctype.h>
#include <stdint.h>
#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <sys/select.h>

#include "debug.h"
#include "config.h"

#define DEV_INPUT_EVENT "/dev/input"
#define test_bit(bit, array) (array[bit / 8] & (1 << (bit % 8)))

pthread_t threadID[256];
int threadCount = 0;
int threadsRunning = 1;

struct MappingThreadArguments
{
    char devicePath[MAX_PATH_LENGTH];
    EVInputs inputs;
    int wiiMode;
};

void *deviceThread(void *_args)
{
    struct MappingThreadArguments *args = (struct MappingThreadArguments *)_args;
    char devicePath[MAX_PATH_LENGTH];
    EVInputs inputs;
    strcpy(devicePath, args->devicePath);
    memcpy(&inputs, &args->inputs, sizeof(EVInputs));
    int wiiMode = args->wiiMode;
    free(args);

    int fd;
    if ((fd = open(devicePath, O_RDONLY)) < 0)
    {
        printf("mapping.c:initDevice(): Failed to open device file descriptor:%d \n", fd);
        exit(-1);
    }

    struct input_event event;

    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    int axisIndex;
    uint8_t absoluteBitmask[ABS_MAX / 8 + 1];
    struct input_absinfo absoluteFeatures;

    memset(absoluteBitmask, 0, sizeof(absoluteBitmask));
    if (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(absoluteBitmask)), absoluteBitmask) < 0)
    {
        perror("evdev ioctl");
    }

    double min = 0, max = 0;
    for (axisIndex = 0; axisIndex < ABS_MAX; ++axisIndex)
    {
        if (test_bit(axisIndex, absoluteBitmask))
        {
            if (ioctl(fd, EVIOCGABS(axisIndex), &absoluteFeatures))
            {
                perror("evdev EVIOCGABS ioctl");
            }
            inputs.absMax[axisIndex] = (double)absoluteFeatures.maximum;
            inputs.absMin[axisIndex] = (double)absoluteFeatures.minimum;
        }
    }

    fd_set file_descriptor;
    struct timeval tv;

    /* Wii Remote Variables */
    int x0 = 0, x1 = 0, y0 = 0, y1 = 0;

    while (threadsRunning)
    {
        bool data_to_read = false;

        FD_ZERO(&file_descriptor);
        FD_SET(fd, &file_descriptor);

        /* set blocking timeout to TIMEOUT_SELECT */
        tv.tv_sec = 0;
        tv.tv_usec = 2 * 1000;

        int n = select(fd + 1, &file_descriptor, NULL, NULL, &tv);
        if (0 == n)
        {
            continue;
        }
        else if (n > 0)
        {
            if (FD_ISSET(fd, &file_descriptor))
            {
                data_to_read = true;
            }
        }

        if (data_to_read && (sizeof(event) == read(fd, &event, sizeof(event))))
        {
            switch (event.type)
            {

            case EV_KEY:
            {
                setSwitch(inputs.key[event.code].jvsPlayer, inputs.key[event.code].output, event.value == 0 ? 0 : 1);
            }
            break;
            case EV_ABS:
            {
                /* Handle the wii remotes differently */
                if (wiiMode)
                {
                    if (event.type == EV_ABS)
                    {
                        switch (event.code)
                        {
                        case 16:
                            x0 = event.value;
                            break;
                        case 17:
                            y0 = event.value;
                            break;
                        case 18:
                            x1 = event.value;
                            break;
                        case 19:
                            y1 = event.value;
                            break;
                        }
                    }

                    if (x0 != 1023 && x1 != 1023 && y0 != 1023 && y1 != 1023)
                    {
                        setSwitch(PLAYER_1, BUTTON_2, 0);
                        int middlex = (int)((x0 + x1) / 2.0);
                        int middley = (int)((y0 + y1) / 2.0);

                        int valuex = middlex;
                        int valuey = 1023 - middley;

                        unsigned char finalX = (unsigned char)(((double)valuex / 1023) * 255.0);
                        unsigned char finalY = (unsigned char)(((double)valuey / 1023) * 255.0);

                        setAnalogue(0, finalX);
                        setAnalogue(1, finalY);
                        setGun(0, finalX);
                        setGun(1, finalY);
                    }
                    else
                    {
                        setSwitch(PLAYER_1, BUTTON_2, 1);
                        setAnalogue(0, 0);
                        setAnalogue(1, 0);
                        setGun(0, 0);
                        setGun(1, 0);
                    }
                    continue;
                }

                /* Handle normally mapped analogue controls */
                if (inputs.absEnabled[event.code])
                {
                    double scaled = ((double)event.value - inputs.absMin[event.code]) / (inputs.absMax[event.code] - inputs.absMin[event.code]);
                    setAnalogue(inputs.abs[event.code].output, inputs.abs[event.code].reverse ? 1 - scaled : scaled);
                    setGun(inputs.abs[event.code].output, inputs.abs[event.code].reverse ? 1 - scaled : scaled);
                }
            }
            break;
            }
        }
    }

    close(fd);

    return 0;
}
void startThread(EVInputs *inputs, char *devicePath, int wiiMode)
{
    struct MappingThreadArguments *args = malloc(sizeof(struct MappingThreadArguments));
    strcpy(args->devicePath, devicePath);
    memcpy(&args->inputs, inputs, sizeof(EVInputs));
    args->wiiMode = wiiMode;
    pthread_create(&threadID[threadCount], NULL, deviceThread, args);
    threadCount++;
}

void stopThreads()
{
    printf("Stopping threads\n");
    threadsRunning = 0;
    for (int i = 0; i < threadCount; i++)
    {
        pthread_join(threadID[i], NULL);
    }
}

int evDevFromString(char *evDevString)
{
    for (int i = 0; i < sizeof(evDevConversion) / sizeof(evDevConversion[0]); i++)
    {
        if (strcmp(evDevConversion[i].string, evDevString) == 0)
        {
            return evDevConversion[i].number;
        }
    }
    debug(0, "Error: Could not find the EV DEV string specified for %s\n", evDevString);
    return -1;
}

ControllerInput controllerInputFromString(char *controllerInputString)
{
    for (long unsigned int i = 0; i < sizeof(controllerInputConversion) / sizeof(controllerInputConversion[0]); i++)
    {
        if (strcmp(controllerInputConversion[i].string, controllerInputString) == 0)
            return controllerInputConversion[i].input;
    }
    debug(0, "Error: Could not find the CONTROLLER INPUT string specified for %s\n", controllerInputString);
    return -1;
}

ControllerPlayer controllerPlayerFromString(char *controllerPlayerString)
{
    for (long unsigned int i = 0; i < sizeof(controllerPlayerConversion) / sizeof(controllerPlayerConversion[0]); i++)
    {
        if (strcmp(controllerPlayerConversion[i].string, controllerPlayerString) == 0)
            return controllerPlayerConversion[i].player;
    }
    debug(0, "Error: Could not find the CONTROLLER PLAYER string specified for %s\n", controllerPlayerString);
    return -1;
}

int processMappings(InputMappings *inputMappings, OutputMappings *outputMappings, EVInputs *evInputs, ControllerPlayer player)
{
    for (int i = 0; i < inputMappings->length; i++)
    {
        int found = 0;
        OutputMapping tempMapping;
        for (int j = outputMappings->length - 1; j >= 0; j--)
        {
            if (found)
                break;

            if (outputMappings->mappings[j].input == inputMappings->mappings[i].input && outputMappings->mappings[j].controllerPlayer == player)
            {
                tempMapping = outputMappings->mappings[j];
                tempMapping.reverse ^= inputMappings->mappings[i].reverse;
                found = 1;
                break;
            }
        }

        if (!found)
        {
            debug(1, "Error: no outside mapping found for %d\n", inputMappings->mappings[i].input);
            continue;
        }

        if (inputMappings->mappings[i].type == ANALOGUE)
        {
            evInputs->abs[inputMappings->mappings[i].code] = tempMapping;
            evInputs->absEnabled[inputMappings->mappings[i].code] = 1;
        }
        else if (inputMappings->mappings[i].type == SWITCH)
        {
            evInputs->key[inputMappings->mappings[i].code] = tempMapping;
            evInputs->absEnabled[inputMappings->mappings[i].code] = 1;
        }
    }
    return 1;
}

int isEventDevice(const struct dirent *dir)
{
    return strncmp("event", dir->d_name, 5) == 0;
}

int getInputs(DeviceList *deviceList)
{
    struct dirent **namelist;

    deviceList->length = 0;

    int numberOfDevices;
    if ((numberOfDevices = scandir(DEV_INPUT_EVENT, &namelist, isEventDevice, alphasort)) < 1)
    {
        debug(0, "Error: No devices found\n");
        return 0;
    }

    for (int i = 0; i < numberOfDevices; i++)
    {
        char fname[512];
        int fd = -1;
        char name[256] = "???";

        snprintf(fname, sizeof(fname), "%s/%s", DEV_INPUT_EVENT, namelist[i]->d_name);

        if ((fd = open(fname, O_RDONLY)) > -1)
        {
            ioctl(fd, EVIOCGNAME(sizeof(name)), name);
            close(fd);
        }

        strcpy(deviceList->devices[deviceList->length].fullName, name);

        for (int i = 0; i < (int)strlen(name); i++)
        {
            name[i] = tolower(name[i]);
            if (name[i] == ' ' || name[i] == '/' || name[i] == '(' || name[i] == ')')
            {
                name[i] = '-';
            }
        }

        strcpy(deviceList->devices[deviceList->length].name, name);
        strcpy(deviceList->devices[deviceList->length].path, fname);
        deviceList->length++;
        free(namelist[i]);
    }
    free(namelist);

    return 1;
}

int initInputs(char *outputMappingPath)
{

    DeviceList deviceList;
    if (!getInputs(&deviceList))
    {
        debug(0, "Error: Failed to open devices\n");
        return 0;
    }

    OutputMappings outputMappings;
    if (parseOutputMapping(outputMappingPath, &outputMappings) != JVS_CONFIG_STATUS_SUCCESS)
    {
        debug(0, "Error: Cannot find an output mapping\n");
        return 0;
    }

    int playerNumber = 1;
    for (int i = 0; i < deviceList.length; i++)
    {
        InputMappings inputMappings;
        inputMappings.length = 0;

        if (parseInputMapping(deviceList.devices[i].name, &inputMappings) != JVS_CONFIG_STATUS_SUCCESS || inputMappings.length == 0)
            continue;

        EVInputs evInputs = (EVInputs){0};

        if (!processMappings(&inputMappings, &outputMappings, &evInputs, (ControllerPlayer)playerNumber))
        {
            debug(0, "Failed to process the mapping for %s\n", deviceList.devices[i].name);
        }
        else
        {
            /* Start the one before it in wii mode (event-1 is usually the ir)*/
            if (strcmp(deviceList.devices[i].name, "nintendo-wii-remote") == 0)
            {
                startThread(&evInputs, deviceList.devices[i - 1].path, 1);
            }

            startThread(&evInputs, deviceList.devices[i].path, 0);
            debug(0, "  Player %d:\t\t%s\n", playerNumber, deviceList.devices[i].fullName);
            playerNumber++;
        }
    }

    return 1;
}