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

#define HOLD_REG_BOOTLOADER_VERSION 330
#define BOOTLOADER_VERSION_LEN      8
#define HOLD_REG_FIRMWARE_SIGNATURE 290
#define FW_SIG_LEN                  12
#define HOLD_REG_FIRMWARE_VERSION   250
#define FW_VERSION_LEN              15

#define BL_MINIMAL_RESPONSE_TIMEOUT    5.0

#define xstr(a) str(a)
#define str(a) #a

const char flashingExample[] = "-d <port> -f <firmware.wbfw>";
const char casualUsageExample[] = "-d <port> -a <modbus_addr> -j -f <firmware.wbfw>";

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

char *mbReadString(modbus_t *ctx, int startAddr, int len);

int probeConnection(modbus_t *ctx);

int printDeviceInfo(modbus_t *ctx);

struct timeval parseResponseTimeout(float timeoutSec);

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
        printf("-j     Jump to bootloader using reg 129                          -\n");
        printf("         uses 9600N2 for communicate with bootloader (can be changed with -B key)\n");
        printf("-J     Jump to bootloader using reg 131                          -\n");
        printf("         uses baudrate from -b key to communicate with bootloader (don`t use -B)\n");
        printf("-u     Reset UART setting and MODBUS address to factory default  -\n");
        printf("-w     Reset device settings stored in flash to factory default  -\n");
        printf("-D     Debug mode                                                -\n");
        printf("-b     Baudrate used to communicate with firmware                9600\n");
        printf("-B     Baudrate used to communicate with bootloader              9600\n");
        printf("-p     Parity                                                    N\n");
        printf("-t     Slave response timeout (in seconds)                       10.0\n");

        printf("\nExamples:\n\n");

        printf("Flashing device that is in bootloader:\n");
        printf("    %s %s\n", argv[0], flashingExample);
        printf("    useful for flashing device immediately after power on\n\n");

        printf("Reset uart settings:\n");
        printf("    %s -d <port> -a10 -u\n\n", argv[0]);

        printf("Flashing running device:\n");
        printf("    %s %s\n\n", argv[0], casualUsageExample);

        printf("Flashing running device using custom baudrate:\n");
        printf("    %s -d <port> -a <modbus_addr> -b115200 -J -f <firmware.wbfw>\n", argv[0]);
        printf("    useful for flashing device behind Modbus-TCP gateway\n\n");

        printf("Only read device info (no flashing):\n");
        printf("    %s -d <port> -a10 --get-device-info\n\n", argv[0]);

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
    int   onlyReadInfo = 0;
    int   uartResetCmd = 0;
    int   eepromFormatCmd = 0;
    int   falshFsFormatCmd = 0;
    int   debug    = 0;
    int   inBootloader = 0;
    float responseTimeout = 10.0f; // Seconds

    const struct option longOptions[] = {
		{ "get-device-info", no_argument, &onlyReadInfo, 1 },
		{ NULL, 0, NULL, 0}
	};

    int c;
    int stopbits;
    while ((c = getopt_long(argc, argv, "d:f:a:t:jJuewDb:p:s:B:", longOptions, NULL)) != -1) {
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
        case '?':
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
        size_t startPos = 0, endPos = strlen(device);

        for (startPos=0;
            (startPos < strlen(device)) && ((device[startPos] == '.') || (device[startPos] == '\\'));
            ++startPos) {};

        for (endPos=strlen(device) - 1;
            (endPos >=0) && (device[endPos] == ':');
            --endPos) {};

        char deviceStripped[32] = {};
        strncpy(deviceStripped, device + startPos, min(sizeof(deviceStripped) - 1, endPos - startPos + 1));

        char buffer[40] = "\\\\.\\";
        strncpy(buffer + strlen(buffer), deviceStripped, sizeof(buffer) - strlen(buffer));

        device = buffer;
    }
#endif

    if (device == NULL) {
        printf("A port should be specified!\n%s -d <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    //Connecting on device's params
    modbus_t *deviceParamsConnection = initModbus(device, deviceParams, modbusID, debug, responseTimeout);

    printf("%s opened successfully.\n", device);

    if (jumpCmdStandardBaud) {
        printf("Send jump to bootloader command and wait 2 seconds...\n");
        if (modbus_write_register(deviceParamsConnection, HOLD_REG_JUMP_TO_BOOT_STANDARD_BAUD, 1) == 1) {
            printf("Ok, device will jump to bootloader.\n");
            inBootloader = 1;
        } else {
            printf("Error: %s.\n", modbus_strerror(errno));
            if ((errno == EMBXILADD) ||
                (errno == EMBXILVAL))  // some of ours fw report illegal data value on nonexistent register
            {
                fprintf(stderr, "Device probably doesn't support in-field firmware upgrade\n");
                deinitModbus(deviceParamsConnection);
                exit(EXIT_FAILURE);
            }
            //Devices firmwares have bug: writing to  HOLD_REG_JUMP_TO_BOOTLOADER at low BDs causes modbus timeout error.
            //"1" in HOLD_REG_JUMP_TO_BOOTLOADER causes reboot to bootloader, and device have ~5ms to send a responce
            printf("May be device already in bootloader, check status led\n");
        }
        sleep(2);    // wait 2 seconds
    } else if (jumpCmdCurrentBaud) {
        printf("Try to jump to bootloader using current baudrate...\n");
        if (modbus_write_register(deviceParamsConnection, HOLD_REG_JUMP_TO_BOOT_CURRENT_BAUD, 1) == 1) {
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
            deinitModbus(deviceParamsConnection);
            exit(EXIT_FAILURE);
        }
        sleep(2);
    }
    deinitModbus(deviceParamsConnection);

    float blResponseTimeout = (BL_MINIMAL_RESPONSE_TIMEOUT > responseTimeout) ? BL_MINIMAL_RESPONSE_TIMEOUT : responseTimeout;

    if (onlyReadInfo) {
        modbus_t *readInfoConnection;
        if (inBootloader) {
            struct UartSettings params = (jumpCmdCurrentBaud) ? deviceParams : bootloaderParams;
            readInfoConnection = initModbus(device, params, modbusID, debug, blResponseTimeout);
            if (probeConnection(readInfoConnection) < 0) {
                fprintf(stderr, "Failed to connect (%d %s): %s\n", modbusID, device, modbus_strerror(errno));
                deinitModbus(readInfoConnection);
                exit(EXIT_FAILURE);
            }
        } else {  // We do not know actual device's state
            readInfoConnection = initModbus(device, deviceParams, modbusID, debug, responseTimeout);
            if (probeConnection(readInfoConnection) < 0) {
                printf("Trying to probe (%d %s) at bootloader params...\n", modbusID, device);
                deinitModbus(readInfoConnection);
                readInfoConnection = initModbus(device, bootloaderParams, modbusID, debug, blResponseTimeout);
                if (probeConnection(readInfoConnection) < 0) {
                    fprintf(stderr, "Failed to connect (%d %s) at bootloader settings: %s\n", modbusID, device, modbus_strerror(errno));
                    deinitModbus(readInfoConnection);
                    exit(EXIT_FAILURE);
                }
            }
        }
        int rc = printDeviceInfo(readInfoConnection);
        deinitModbus(readInfoConnection);
        if (rc < 0) {
            exit(EXIT_FAILURE);
        }
        exit(EXIT_SUCCESS);
    }

    modbus_t *bootloaderParamsConnection;
    if (jumpCmdCurrentBaud) {
        bootloaderParamsConnection = initModbus(device, deviceParams, modbusID, debug, blResponseTimeout);
    } else {
        //Connecting on Bootloader's params
        bootloaderParamsConnection = initModbus(device, bootloaderParams, modbusID, debug, blResponseTimeout);
    }

    if (uartResetCmd) {
        printf("Send reset UART settings and modbus address command...\n");
        if (modbus_write_register(bootloaderParamsConnection, HOLD_REG_CMD_UART_SETTINGS_RESET, 1) == 1) {
            printf("Ok.\n");
            inBootloader = 1;
        } else {
            printf("Error: %s.\n", modbus_strerror(errno));
        }
        sleep(1);    // wait 1 second
    }

    if (eepromFormatCmd) {
        printf("Send format EEPROM command...\n");
        if (modbus_write_register(bootloaderParamsConnection, HOLD_REG_CMD_EEPROM_ERASE, 1) == 1) {
            printf("Ok.\n");
            inBootloader = 1;
        } else {
            printf("Error: %s.\n", modbus_strerror(errno));
        }
        sleep(1);    // wait 1 second
    }

    if (falshFsFormatCmd) {
        printf("Send format FlashFS command...\n");
        if (modbus_write_register(bootloaderParamsConnection, HOLD_REG_CMD_FLASHFS_ERASE, 1) == 1) {
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
        deinitModbus(bootloaderParamsConnection);
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
        if (modbus_write_registers(bootloaderParamsConnection, INFO_BLOCK_REG_ADDRESS, INFO_BLOCK_SIZE / 2, &data[filePointer / 2]) == (INFO_BLOCK_SIZE / 2)) {
            printf(" OK\n"); fflush(stdout);
            filePointer += INFO_BLOCK_SIZE;
            break;
        }
        printf("\n"); fflush(stdout);
        fprintf(stderr, "Error while sending info block: %s\n", modbus_strerror(errno));
        if (errno == EMBXSFAIL) {
            fprintf(stderr, "Data format is invalid or firmware signature doesn't match the device\n");
            deinitModbus(bootloaderParamsConnection);
            exit(EXIT_FAILURE);
        } else if ((errno == EMBXILADD) ||
                   (errno == EMBXILVAL))  // some of our fws report illegal data value on nonexistent register
        {
            fprintf(stderr, "Not in bootloader mode? Try repeating with -j\n");
            deinitModbus(bootloaderParamsConnection);
            exit(EXIT_FAILURE);
        }
        fflush(stderr);
        sleep(3);
        errorCount++;
        if (errorCount == MAX_ERROR_COUNT) {
            fprintf(stderr, "Error while sending info block.\n");
            fprintf(stderr, "Check connection, jump to bootloader and try again.\n");
            fflush(stderr);
            deinitModbus(bootloaderParamsConnection);
            exit(EXIT_FAILURE);
        }
    }

    printf("\n");
    while (filePointer < filesize) {
        fflush(stdout);
        printf("\rSending data block %u of %u...",
               (filePointer - INFO_BLOCK_SIZE) / DATA_BLOCK_SIZE + 1,
               (filesize - INFO_BLOCK_SIZE) / DATA_BLOCK_SIZE); fflush(stdout);
        if (modbus_write_registers(bootloaderParamsConnection, DATA_BLOCK_REG_ADDRESS, DATA_BLOCK_SIZE / 2, &data[filePointer / 2]) == (DATA_BLOCK_SIZE / 2)) {
            filePointer += DATA_BLOCK_SIZE;
            errorCount = 0;
        } else {
            printf("\n"); fflush(stdout);
            fprintf(stderr, "Error while sending data block: %s\n", modbus_strerror(errno));
            if (errno == EMBXSFAIL) {
                fprintf(stderr, "Firmware file is corrupted?\n");
                deinitModbus(bootloaderParamsConnection);
                exit(EXIT_FAILURE);
            }
            fflush(stderr);
            if (errorCount == MAX_ERROR_COUNT) {
                filePointer += DATA_BLOCK_SIZE;
            }
            if (errorCount >= MAX_ERROR_COUNT * 2) {
                deinitModbus(bootloaderParamsConnection);
                exit(EXIT_FAILURE);
            }
            errorCount++;
        }
    }

    printf(" OK.\n\nAll done!\n");

    deinitModbus(bootloaderParamsConnection);

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
    modbus_t *mbConnection = modbus_new_rtu(device, deviceParams.baudrate, deviceParams.parity, deviceParams.databits, deviceParams.stopbits);
#else
    int stopbitsReceiving = (stopbitsAreForced == 0) ? 1 : deviceParams.stopbits;
    modbus_t *mbConnection = modbus_new_rtu_different_stopbits(device, deviceParams.baudrate, deviceParams.parity, deviceParams.databits, deviceParams.stopbits, stopbitsReceiving);
#endif

    if (mbConnection == NULL) {
        fprintf(stderr, "Unknown error.\n");
        exit(EXIT_FAILURE);
    }

    if (modbus_connect(mbConnection) != 0) {
        fprintf(stderr, "Connection failed: %s\n", modbus_strerror(errno));
        modbus_free(mbConnection);
        exit(EXIT_FAILURE);
    }

    modbus_set_error_recovery(mbConnection, MODBUS_ERROR_RECOVERY_PROTOCOL);

    if (modbus_set_slave(mbConnection, slaveAddr) != 0) {
        if (errno == EINVAL) {
            fprintf(stderr, "Invalid slave id!\nChoose from 0 to 247\n");
        } else {
            fprintf(stderr, "Unknown error on setting slave id.\n");
        }
        modbus_free(mbConnection);
        exit(EXIT_FAILURE);
    };

    struct timeval timeout = parseResponseTimeout(responseTimeout);
    setResponseTimeout(timeout, mbConnection);

    modbus_flush(mbConnection);
    modbus_set_debug(mbConnection, debug);
    return mbConnection;
}

void deinitModbus(modbus_t *modbusConnection){
    modbus_close(modbusConnection);
    modbus_free(modbusConnection);
}

char *mbReadString(modbus_t *ctx, int startAddr, int len){
    uint16_t vals[len];
    int rc = modbus_read_registers(ctx, startAddr, len, vals);
    if (rc >= 0) {
        char *buf = malloc(len + 1);
        for (int i=0; i < rc; i++) {
            buf[i] = (char)vals[i];
        }
        buf[rc] = '\0';
        return buf;
    }
    return NULL;
}

int probeConnection(modbus_t *ctx){
    uint16_t firmwareSignature[FW_SIG_LEN];  // reading fw-sig is supported both in firmware and bootloader
    return modbus_read_registers(ctx, HOLD_REG_FIRMWARE_SIGNATURE, FW_SIG_LEN, firmwareSignature);
}

int printDeviceInfo(modbus_t *ctx){
    int rc = 0;

    char *bootloaderVersion = mbReadString(ctx, HOLD_REG_BOOTLOADER_VERSION, BOOTLOADER_VERSION_LEN);
    if (bootloaderVersion == NULL){
        printf("Bootloader version read error: %s\n", modbus_strerror(errno));
        rc = errno;
    } else {
        printf("Bootloader version: %s\n", bootloaderVersion);
    }
    free(bootloaderVersion);

    char *firmwareVersion = mbReadString(ctx, HOLD_REG_FIRMWARE_VERSION, FW_VERSION_LEN);
    if (firmwareVersion == NULL){
        printf("Firmware version read error: %s; Maybe device is in bootloader?\n", modbus_strerror(errno));
        // do not set rc: bootloader cannot read fw-version
    } else {
        printf("Firmware version: %s\n", firmwareVersion);
    }
    free(firmwareVersion);

    char *firmwareSignature = mbReadString(ctx, HOLD_REG_FIRMWARE_SIGNATURE, FW_SIG_LEN);
    if (firmwareSignature == NULL){
        printf("Firmware signature (fw-sig) read error: %s\n", modbus_strerror(errno));
        rc = errno;
    } else {
        printf("Firmware signature (fw-sig): %s\nDownload firmwares: https://fw-releases.wirenboard.com/?prefix=fw/by-signature/%s/\n", firmwareSignature, firmwareSignature);
    }
    free(firmwareSignature);

    return rc;
}

struct timeval parseResponseTimeout(float timeoutSec) {
    long decimalPart = (long)timeoutSec;
    float fractPart = timeoutSec - decimalPart;
    struct timeval responseTimeout;
    responseTimeout.tv_sec = decimalPart;
    responseTimeout.tv_usec = (long)(fractPart * 1000000); // Microseconds
    return responseTimeout;
}

void setResponseTimeout(struct timeval timeoutStruct, modbus_t *modbusContext){
    #if LIBMODBUS_VERSION_CHECK(3, 1, 2)
        modbus_set_response_timeout(modbusContext, timeoutStruct.tv_sec, timeoutStruct.tv_usec);
    #else
        modbus_set_response_timeout(modbusContext, &timeoutStruct);
    #endif
}
