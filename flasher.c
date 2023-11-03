#include <stdio.h>
#include <stdlib.h>
#include <modbus.h>
#include <errno.h>
#include <getopt.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>

#define INFO_BLOCK_SIZE             32
#define INFO_BLOCK_REG_ADDRESS      0x1000

#define DATA_BLOCK_SIZE             136
#define DATA_BLOCK_REG_ADDRESS      0x2000

#define MAX_ERROR_COUNT             3

#define HOLD_REG_JUMP_TO_BOOT_STANDARD_BAUD 129
#define HOLD_REG_JUMP_TO_BOOT_CURRENT_BAUD  131
#define HOLD_REG_CMD_UART_SETTINGS_RESET    1000
#define HOLD_REG_CMD_EEPROM_ERASE           1001
#define HOLD_REG_CMD_FLASHFS_ERASE          1002

#define BL_MINIMAL_RESPONSE_TIMEOUT    5.0

#define xstr(a) str(a)
#define str(a) #a

const char flashingExample[] = "-d <port> -f <firmware.wbfw>";
const char formatSettingsExample[] = "-d <port> -j -u";
const char casualUsageExample[] = "-d <port> -a <modbus_addr> -j -u -f <firmware.wbfw>";

struct UartSettings {
    int baudrate;
    char parity;
    int databits;
    int stopbits;
} UartSettings;

const int allowedBaudrates[] = {1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400};
const int allowedStopBits[] = {1, 2};
const char allowedParity[] = {'N', 'E', 'O'};

int ensureIntIn(int param, const int array[], unsigned int arrayLen);
int ensureCharIn(char param, const char array[], unsigned int arrayLen);

modbus_t *initModbus(char *device, struct UartSettings deviceParams, int slaveAddr, int debug, float responseTimeout);

void deinitModbus(modbus_t *modbusConnection);

struct timeval parseResponseTimeout(float timeout_sec);

void setResponseTimeout(struct timeval timeoutStruct, modbus_t *modbusContext);

int stopbitsAreForced = 0;

int main(int argc, char *argv[])
{
    if (argc == 1) {
        printf("Welcome to Wiren Board flash tool.\n");
        printf("Version: " xstr(VERSION) ", libmodbus version: " LIBMODBUS_VERSION_STRING "\n\n");
        printf("Usage:\n\n");

        printf("Param  Description                                         Default value\n\n");
#if defined(_WIN32)
        printf("-d     Serial port (\"COMxx\", e.g. COM12)                         -\n");
        printf("-s     Stopbits (2/1)                                              2\n");
#else
        printf("-d     Serial port (e.g. \"/dev/ttyRS485-1\")                      -\n");
        printf("-s     Stopbits (2/1)                                   auto: (2sb->, ->1sb)\n");
#endif
        printf("-f     Firmware file                                             -\n");
        printf("-a     Modbus ID (slave addr)                                    1\n");
        printf("-j     Jump to bootloader using reg 129 (9600N2)                 -\n");
        printf("-J     Jump to bootloader using reg 131 (current baudrate)       -\n");
        printf("-u     Reset UART setting and MODBUS address to factory default  -\n");
        printf("-w     Reset device settings stored in flash to factory default  -\n");
        printf("-D     Debug mode                                                -\n");
        printf("-b     Baudrate used to communicate with firmware                9600\n");
        printf("-B     Baudrate used to communicate with bootloader              9600\n");
        printf("-p     Parity                                                    N\n");
        printf("-t     Slave response timeout (in seconds)                       10.0\n");

        printf("\nMinimal flashing example:\n%s %s\n", argv[0], flashingExample);
        printf("Minimal format uart settings example:\n%s %s\n", argv[0], formatSettingsExample);
        printf("Flashing running device example:\n%s %s\n", argv[0], casualUsageExample);
        return 0;
    };

    struct UartSettings bootloaderParams = { //Bootloader has fast flash mode with high baudrate
        .baudrate = 9600,
        .parity = 'N',
        .databits = 8,
        .stopbits = 2
    };

    struct UartSettings deviceParams = { //To send -j to device. Filled from user input
        .baudrate = 9600,
        .parity = 'N',
        .databits = 8,
        .stopbits = 2
    };

    // Default values
    char *device   = NULL;
    char *fileName = NULL;
    int   modbusID = 1;
    int   jumpCmdStandardBaud = 0;
    int   jumpCmdCurrentBaud = 0;
    int   uartResetCmd = 0;
    int   eepromFormatCmd = 0;
    int   falshFsFormatCmd = 0;
    int   debug    = 0;
    int   inBootloader = 0;
    float responseTimeout = 10.0f; // Seconds


    int c;
    int stopbits;
    while ((c = getopt(argc, argv, "d:f:a:t:jJuewDb:p:s:B:")) != -1) {
        switch (c) {
        case 'd':
            device = optarg;
            break;
        case 'f':
            fileName = optarg;
            break;
        case 'a':
            sscanf(optarg, "%d", &modbusID);
            break;
        case 't':
            sscanf(optarg, "%f", &responseTimeout);
            if (responseTimeout >= 0) {
                break;
            } else {
                printf("Response timeout (-t <%f>) could not be less zero!\n", responseTimeout);
                exit(EXIT_FAILURE);
            };
        case 'j':
            jumpCmdStandardBaud = 1;
            break;
        case 'J':
            jumpCmdCurrentBaud = 1;
            break;
        case 'u':
            uartResetCmd = 1;
            break;
        case 'e':
            eepromFormatCmd = 1;
            break;
        case 'w':
            falshFsFormatCmd = 1;
            break;
        case 'D':
            debug = 1;
            break;
        case 'b':
            sscanf(optarg, "%d", &deviceParams.baudrate);
            if (ensureIntIn(deviceParams.baudrate, allowedBaudrates, sizeof(allowedBaudrates))) {
                break;
            } else {
                printf("Baudrate (-b <%d>) is not supported!\n", deviceParams.baudrate);
                exit(EXIT_FAILURE);
            };
        case 'B':
            sscanf(optarg, "%d", &bootloaderParams.baudrate);
            if (ensureIntIn(bootloaderParams.baudrate, allowedBaudrates, sizeof(allowedBaudrates))) {
                break;
            } else {
                printf("Baudrate (-B <%d>) is not supported!\n", bootloaderParams.baudrate);
                exit(EXIT_FAILURE);
            };
        case 'p':
            sscanf(optarg, "%c", &deviceParams.parity);
            if (ensureCharIn(deviceParams.parity, allowedParity, sizeof(allowedParity))) {
                break;
            } else {
                printf("Parity (-p <%c>) is not supported!\n", deviceParams.parity);
                exit(EXIT_FAILURE);
            };
        case 's':
        /*
        -s arg provides stopbits for both alive-device & in-bootloader connections;
        Defaults:
            WIN32 - 2sb
            Posix - auto stopbits (2sb->, ->1sb)
        */
            sscanf(optarg, "%d", &stopbits);
            if (ensureIntIn(stopbits, allowedStopBits, sizeof(allowedStopBits))) {
                stopbitsAreForced = 1;
                deviceParams.stopbits = stopbits;
                bootloaderParams.stopbits = stopbits;
                break;
            } else {
                printf ("Stopbits (-s <%d>) are not supported!\n", stopbits);
                exit(EXIT_FAILURE);
            };
        default:
            printf("Parameters error.\n");
            break;
        }
    }

    if (jumpCmdStandardBaud && jumpCmdCurrentBaud) {
        printf("Parameters error.\n");
        printf("You can't use -j and -J at the same time.\n");
        exit(EXIT_FAILURE);
    }

#if defined(_WIN32)
    // We expect device in a form of "COMxx". So strip leading "." and "\", and trailing ":".
    if (device) {
        size_t start_pos = 0, end_pos = strlen(device);

        for (start_pos=0;
            (start_pos < strlen(device)) && ((device[start_pos] == '.') || (device[start_pos] == '\\'));
            ++start_pos) {};

        for (end_pos=strlen(device) - 1;
            (end_pos >=0) && (device[end_pos] == ':');
            --end_pos) {};

        char device_stripped[32] = {};
        strncpy(device_stripped, device + start_pos, min(sizeof(device_stripped) - 1, end_pos - start_pos + 1));

        char buffer[40] = "\\\\.\\";
        strncpy(buffer + strlen(buffer), device_stripped, sizeof(buffer) - strlen(buffer));

        device = buffer;
    }
#endif

    if (device == NULL) {
        printf("A port should be specified!\n%s -d <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    //Connecting on device's params
    modbus_t *device_params_connection = initModbus(device, deviceParams, modbusID, debug, responseTimeout);

    printf("%s opened successfully.\n", device);

    if (jumpCmdStandardBaud) {
        printf("Send jump to bootloader command and wait 2 seconds...\n");
        if (modbus_write_register(device_params_connection, HOLD_REG_JUMP_TO_BOOT_STANDARD_BAUD, 1) == 1) {
            printf("Ok, device will jump to bootloader.\n");
            inBootloader = 1;
        } else {
            printf("Error: %s.\n", modbus_strerror(errno));
            if ((errno == EMBXILADD) ||
                (errno == EMBXILVAL))  // some of ours fw report illegal data value on nonexistent register
            {
                fprintf(stderr, "Device probably doesn't support in-field firmware upgrade\n");
                deinitModbus(device_params_connection);
                exit(EXIT_FAILURE);
            }
            //Devices firmwares have bug: writing to  HOLD_REG_JUMP_TO_BOOTLOADER at low BDs causes modbus timeout error.
            //"1" in HOLD_REG_JUMP_TO_BOOTLOADER causes reboot to bootloader, and device have ~5ms to send a responce
            printf("May be device already in bootloader, check status led\n");
        }
        sleep(2);    // wait 2 seconds
    } else if (jumpCmdCurrentBaud) {
        printf("Try to jump to bootloader using current baudrate...\n");
        if (modbus_write_register(device_params_connection, HOLD_REG_JUMP_TO_BOOT_CURRENT_BAUD, 1) == 1) {
            printf("Ok, device supports this. Baudrate %d will be used for flashing.\n", deviceParams.baudrate);
            inBootloader = 1;
        } else {
            fprintf(stderr, "Error while writing register %d: %s.\n", HOLD_REG_JUMP_TO_BOOT_CURRENT_BAUD, modbus_strerror(errno));
            if (errno == EMBXILADD) {
                fprintf(stderr, "Firmware and/or bootloader doesn't support this command. Please upgrade firmware and/or bootloader.\n");
                fprintf(stderr, "Alternatively, you can use -j option to jump to bootloader using standard baudrate.\n");
            } else {
                fprintf(stderr, "Other error, check device connection parameters.\n");
            }
            deinitModbus(device_params_connection);
            exit(EXIT_FAILURE);
        }
        sleep(2);
    }

    deinitModbus(device_params_connection);

    float bl_response_timeout = (BL_MINIMAL_RESPONSE_TIMEOUT > responseTimeout) ? BL_MINIMAL_RESPONSE_TIMEOUT : responseTimeout;
    modbus_t *bootloader_params_connection;
    if (jumpCmdCurrentBaud) {
        bootloader_params_connection = initModbus(device, deviceParams, modbusID, debug, bl_response_timeout);
    } else {
        //Connecting on Bootloader's params
        bootloader_params_connection = initModbus(device, bootloaderParams, modbusID, debug, bl_response_timeout);
    }

    if (uartResetCmd) {
        printf("Send reset UART settings and modbus address command...\n");
        if (modbus_write_register(bootloader_params_connection, HOLD_REG_CMD_UART_SETTINGS_RESET, 1) == 1) {
            printf("Ok.\n");
            inBootloader = 1;
        } else {
            printf("Error: %s.\n", modbus_strerror(errno));
        }
        sleep(1);    // wait 1 second
    }

    if (eepromFormatCmd) {
        printf("Send format EEPROM command...\n");
        if (modbus_write_register(bootloader_params_connection, HOLD_REG_CMD_EEPROM_ERASE, 1) == 1) {
            printf("Ok.\n");
            inBootloader = 1;
        } else {
            printf("Error: %s.\n", modbus_strerror(errno));
        }
        sleep(1);    // wait 1 second
    }

    if (falshFsFormatCmd) {
        printf("Send format FlashFS command...\n");
        if (modbus_write_register(bootloader_params_connection, HOLD_REG_CMD_FLASHFS_ERASE, 1) == 1) {
            printf("Ok.\n");
            inBootloader = 1;
        } else {
            printf("Error: %s.\n", modbus_strerror(errno));
        }
        sleep(1);    // wait 1 second
    }

    if (fileName == NULL) {
        if (inBootloader) {
            printf ("Device is in Bootloader now! To flash FW run\n%s %s\n", argv[0], flashingExample);
        } else {
            printf("To flash FW on running device, run\n%s %s\n", argv[0], casualUsageExample);
        }
        return 0;
    }

    FILE *file = fopen(fileName, "rb");
    if (file == NULL) {
        fprintf(stderr, "Error while opening firmware file: %s\n", strerror(errno));
        deinitModbus(bootloader_params_connection);
        exit(EXIT_FAILURE);
    }

    fseek(file, 0L, SEEK_END);
    unsigned int filesize = ftell(file);
    printf("%s opened successfully, size %d bytes\n", fileName, filesize);
    rewind(file);

    uint16_t *data = malloc(filesize);
    if (fread(data, 1, filesize, file) != filesize) {
        fprintf(stderr, "Error while reading firmware file: %s\n", strerror(errno));
    }

    for (unsigned int i = 0; i < filesize / 2; i++) {
        data[i] = ((data[i] & 0xFF) << 8) | ((data[i] & 0xFF00) >> 8);
    }

    int errorCount = 0;
    unsigned int filePointer = 0;

    printf("\nSending info block...");
    while (errorCount < MAX_ERROR_COUNT) {
        if (modbus_write_registers(bootloader_params_connection, INFO_BLOCK_REG_ADDRESS, INFO_BLOCK_SIZE / 2, &data[filePointer / 2]) == (INFO_BLOCK_SIZE / 2)) {
            printf(" OK\n"); fflush(stdout);
            filePointer += INFO_BLOCK_SIZE;
            break;
        }
        printf("\n"); fflush(stdout);
        fprintf(stderr, "Error while sending info block: %s\n", modbus_strerror(errno));
        if (errno == EMBXSFAIL) {
            fprintf(stderr, "Data format is invalid or firmware signature doesn't match the device\n");
            deinitModbus(bootloader_params_connection);
            exit(EXIT_FAILURE);
        } else if ((errno == EMBXILADD) ||
                   (errno == EMBXILVAL))  // some of our fws report illegal data value on nonexistent register
        {
            fprintf(stderr, "Not in bootloader mode? Try repeating with -j\n");
            deinitModbus(bootloader_params_connection);
            exit(EXIT_FAILURE);
        }
        fflush(stderr);
        sleep(3);
        errorCount++;
        if (errorCount == MAX_ERROR_COUNT) {
            fprintf(stderr, "Error while sending info block.\n");
            fprintf(stderr, "Check connection, jump to bootloader and try again.\n");
            fflush(stderr);
            deinitModbus(bootloader_params_connection);
            exit(EXIT_FAILURE);
        }
    }

    printf("\n");
    while (filePointer < filesize) {
        fflush(stdout);
        printf("\rSending data block %u of %u...",
               (filePointer - INFO_BLOCK_SIZE) / DATA_BLOCK_SIZE + 1,
               (filesize - INFO_BLOCK_SIZE) / DATA_BLOCK_SIZE); fflush(stdout);
        if (modbus_write_registers(bootloader_params_connection, DATA_BLOCK_REG_ADDRESS, DATA_BLOCK_SIZE / 2, &data[filePointer / 2]) == (DATA_BLOCK_SIZE / 2)) {
            filePointer += DATA_BLOCK_SIZE;
            errorCount = 0;
        } else {
            printf("\n"); fflush(stdout);
            fprintf(stderr, "Error while sending data block: %s\n", modbus_strerror(errno));
            if (errno == EMBXSFAIL) {
                fprintf(stderr, "Firmware file is corrupted?\n");
                deinitModbus(bootloader_params_connection);
                exit(EXIT_FAILURE);
            }
            fflush(stderr);
            if (errorCount == MAX_ERROR_COUNT) {
                filePointer += DATA_BLOCK_SIZE;
            }
            if (errorCount >= MAX_ERROR_COUNT * 2) {
                deinitModbus(bootloader_params_connection);
                exit(EXIT_FAILURE);
            }
            errorCount++;
        }
    }

    printf(" OK.\n\nAll done!\n");

    deinitModbus(bootloader_params_connection);

    fclose(file);
    free(data);

    exit(EXIT_SUCCESS);
}

int ensureIntIn(int param, const int array[], unsigned int arrayLen) {
    int valueIsIn = 0;
    for (unsigned int i = 0; i < arrayLen; i++){
        if (param == array[i]){
            valueIsIn = 1;
            break;
        }
    }
    return valueIsIn;
}


int ensureCharIn(char param, const char array[], unsigned int arrayLen) {
    int valueIsIn = 0;
    for (unsigned int i = 0; i < arrayLen; i++){
        if (param == array[i]){
            valueIsIn = 1;
            break;
        }
    }
    return valueIsIn;
}

modbus_t *initModbus(char *device, struct UartSettings deviceParams, int slaveAddr, int debug, float responseTimeout){
#if defined(_WIN32)  // different stopbits for receiving & transmitting are supported only in posix
    modbus_t *mb_connection = modbus_new_rtu(device, deviceParams.baudrate, deviceParams.parity, deviceParams.databits, deviceParams.stopbits);
#else
    int stopbitsReceiving = (stopbitsAreForced == 0) ? 1 : deviceParams.stopbits;
    modbus_t *mb_connection = modbus_new_rtu_different_stopbits(device, deviceParams.baudrate, deviceParams.parity, deviceParams.databits, deviceParams.stopbits, stopbitsReceiving);
#endif

    if (mb_connection == NULL) {
        fprintf(stderr, "Unknown error.\n");
        exit(EXIT_FAILURE);
    }

    if (modbus_connect(mb_connection) != 0) {
        fprintf(stderr, "Connection failed: %s\n", modbus_strerror(errno));
        modbus_free(mb_connection);
        exit(EXIT_FAILURE);
    }

    modbus_set_error_recovery(mb_connection, MODBUS_ERROR_RECOVERY_PROTOCOL);

    if (modbus_set_slave(mb_connection, slaveAddr) != 0) {
        if (errno == EINVAL) {
            fprintf(stderr, "Invalid slave id!\nChoose from 0 to 247\n");
        } else {
            fprintf(stderr, "Unknown error on setting slave id.\n");
        }
        modbus_free(mb_connection);
        exit(EXIT_FAILURE);
    };

    struct timeval response_timeout = parseResponseTimeout(responseTimeout);
    setResponseTimeout(response_timeout, mb_connection);

    modbus_flush(mb_connection);
    modbus_set_debug(mb_connection, debug);
    return mb_connection;
}

void deinitModbus(modbus_t *modbusConnection){
    modbus_close(modbusConnection);
    modbus_free(modbusConnection);
}

struct timeval parseResponseTimeout(float timeout_sec) {
    long decimal_part = (long)timeout_sec;
    float fract_part = timeout_sec - decimal_part;
    struct timeval response_timeout;
    response_timeout.tv_sec = decimal_part;
    response_timeout.tv_usec = (long)(fract_part * 1000000); // Microseconds
    return response_timeout;
}

void setResponseTimeout(struct timeval timeoutStruct, modbus_t *modbusContext){
    #if LIBMODBUS_VERSION_CHECK(3, 1, 2)
        modbus_set_response_timeout(modbusContext, timeoutStruct.tv_sec, timeoutStruct.tv_usec);
    #else
        modbus_set_response_timeout(modbusContext, &timeoutStruct);
    #endif
}
