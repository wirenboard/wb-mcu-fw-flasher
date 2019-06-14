#include <stdio.h>
#include <stdlib.h>
#include <modbus.h>
#include <errno.h>
#include <getopt.h>
#include <unistd.h>
#include <string.h>

#define INFO_BLOCK_SIZE             32
#define INFO_BLOCK_REG_ADDRESS      0x1000

#define DATA_BLOCK_SIZE             136
#define DATA_BLOCK_REG_ADDRESS      0x2000

#define MAX_ERROR_COUNT             3

#define HOLD_REG_JUMP_TO_BOOTLOADER         129
#define HOLD_REG_CMD_UART_SETTINGS_RESET    1000
#define HOLD_REG_CMD_EEPROM_ERASE           1001

#define xstr(a) str(a)
#define str(a) #a

const char flashingExample[] = "-d <port> -f <firmware.wbfw>";
const char formatSettingsExample[] = "-d <port> -j -u";
const char casualUsage[] = "-d <port> -a <modbus_addr> -j -u -f <firmware.wbfw>";

struct modbusParams {
    int baudrate;
    char parity;
    int databits;
    int stopbits;
} modbusParams;

const int allowedBaudrates[] = {1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200};
const int allowedStopBits[] = {1, 2};
const char allowedParity[] = {'N', 'E', 'O'};

int ensureIntIn(int param, const int array[], unsigned int arrayLen);
int ensureCharIn(char param, const char array[], unsigned int arrayLen);

int main(int argc, char *argv[])
{
    if (argc == 1) {
        printf("Welcome to Wiren Board flash tool.\n");
        printf("Version: " xstr(VERSION) ", libmodbus version: " LIBMODBUS_VERSION_STRING "\n\n");
        printf("Usage:\n\n");

        printf("Param  Description                                               Default value\n\n");
#if defined(_WIN32)
        printf("-d     Serial port (\"COMxx\", e.g. COM12)                         -\n");
#else
        printf("-d     Serial port (e.g. \"/dev/ttyRS485-1\")                      -\n");
#endif
        printf("-f     Firmware file                                             -\n");
        printf("-a     Modbus ID                                                 1\n");
        printf("-j     Send jump to bootloader command                           -\n");
        printf("-u     Reset UART setting and MODBUS address to factory default  -\n");
        printf("-e     Format EEPROM (except device signature)                   -\n");
        printf("-r     Jump to bootloader register address                       129\n");
        printf("-D     Debug mode                                                -\n");
        printf("-b     Baud Rate (serial port speed)                             9600\n");
        printf("-p     Parity                                                    N\n");
        printf("-s     Stopbits                                                  2\n");

        printf("\nMinimal flashing example:\n%s %s\n", argv[0], flashingExample);
        printf("Minimal format uart settings example:\n%s %s\n", argv[0], formatSettingsExample);
        printf("Flashing running device example:\n%s %s\n", argv[0], casualUsage);
        return 0;
    };

    struct modbusParams bootloaderParams = { //Bootloader has fixed uart settings
        .baudrate = 9600,
        .parity = 'N',
        .databits = 8,
        .stopbits = 2
    };

    struct modbusParams deviceParams = { //To send -j to device. Filled from user input
        .baudrate = 9600,
        .parity = 'N',
        .databits = 8,
        .stopbits = 2
    };

    // Default values
    char *device   = NULL;
    char *fileName = NULL;
    int   modbusID = 1;
    int   jumpCmd  = 0;
    int   uartResetCmd = 0;
    int   eepromFormatCmd = 0;
    int   jumpReg  = HOLD_REG_JUMP_TO_BOOTLOADER;
    int   debug    = 0;
    int   inBootloader = 0;

    int c;
    while ((c = getopt(argc, argv, "d:f:a:juer:Db:p:s:")) != -1) {
        switch (c) {
        case 'd':
            device = optarg;
            break;
        case 'f':
            fileName = optarg;
            break;
        case 'a':
            sscanf(optarg, "%d", &modbusID);
            if (modbusID > 247 || modbusID < 0) {
                printf("Incorrect SlaveId!\nChoose from %d to %d\n\n", 0, 247);
                return -1;
            }
            break;
        case 'j':
            jumpCmd = 1;
            break;
        case 'u':
            uartResetCmd = 1;
            break;
        case 'e':
            eepromFormatCmd = 1;
            break;
        case 'r':
            sscanf(optarg, "%d", &jumpReg);
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
                return -1;
            };
        case 'p':
            sscanf(optarg, "%c", &deviceParams.parity);
            if (ensureCharIn(deviceParams.parity, allowedParity, sizeof(allowedParity))) {
                break;
            } else {
                printf("Parity (-p <%c>) is not supported!\n", deviceParams.parity);
                return -1;
            };
        case 's':
            sscanf(optarg, "%d", &deviceParams.stopbits);
            if (ensureIntIn(deviceParams.stopbits, allowedStopBits, sizeof(allowedStopBits))) {
                break;
            } else {
                printf ("Stopbits (-s <%d>) are not supported!\n", deviceParams.stopbits);
                return -1;
            };
        default:
            printf("Parameters error.\n");
            break;
        }
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
        return 0;
    }

    //Connecting on device's params
    modbus_t *mb_connection = modbus_new_rtu(device, deviceParams.baudrate, deviceParams.parity, deviceParams.databits, deviceParams.stopbits);

    if (mb_connection == NULL) {
        fprintf(stderr, "Unknown error.\n");
        return -1;
    }

    if (modbus_connect(mb_connection) != 0) {
        fprintf(stderr, "Connection failed: %s\n", modbus_strerror(errno));
        modbus_free(mb_connection);
        return -1;
    }

    printf("%s opened successfully.\n", device);

    modbus_set_error_recovery(mb_connection, MODBUS_ERROR_RECOVERY_PROTOCOL);

    modbus_set_slave(mb_connection, modbusID);
    modbus_set_debug(mb_connection, debug);

    struct timeval response_timeout;
    response_timeout.tv_sec = 10;
    response_timeout.tv_usec = 0;
    #if LIBMODBUS_VERSION_CHECK(3, 1, 2)
        modbus_set_response_timeout(mb_connection, response_timeout.tv_sec, response_timeout.tv_usec);
    #else
        modbus_set_response_timeout(mb, &response_timeout);
    #endif

    if (jumpCmd) {
        printf("Send jump to bootloader command and wait 2 seconds...\n");
        if (modbus_write_register(mb_connection, jumpReg, 1) == 1) {
            printf("Ok, device will jump to bootloader.\n");
            inBootloader = 1;
        } else {
            printf("Error: %s.\n", modbus_strerror(errno));
            if ((errno == EMBXILADD) || 
                (errno == EMBXILVAL))  // some of ours fw report illegal data value on nonexistent register
            {
                fprintf(stderr, "Device probably doesn't support in-field firmware upgrade\n");
                return -1;
            }
            //Devices firmwares have bug: writing to  HOLD_REG_JUMP_TO_BOOTLOADER at low BDs causes modbus timeout error.
            //"1" in HOLD_REG_JUMP_TO_BOOTLOADER causes reboot to bootloader, and device have ~5ms to send a responce
            printf("May be device already in bootloader, check status led\n");
        }
        sleep(2);    // wait 2 seconds
    }

    modbus_close(mb_connection);
    modbus_free(mb_connection);

    //Connecting on Bootloader's params
    modbus_t *mb = modbus_new_rtu(device, bootloaderParams.baudrate, bootloaderParams.parity, bootloaderParams.databits, bootloaderParams.stopbits);

    if (mb == NULL) {
        fprintf(stderr, "Unknown error.\n");
        return -1;
    }

    if (modbus_connect(mb) != 0) {
        fprintf(stderr, "Connection failed: %s\n", modbus_strerror(errno));
        modbus_free(mb);
        return -1;
    }

    printf("%s opened successfully.\n", device);

    modbus_set_error_recovery(mb, MODBUS_ERROR_RECOVERY_PROTOCOL);

    modbus_set_slave(mb, modbusID);
    modbus_set_debug(mb, debug);

    if (uartResetCmd) {
        printf("Send reset UART settings and modbus address command...\n");
        if (modbus_write_register(mb, HOLD_REG_CMD_UART_SETTINGS_RESET, 1) == 1) {
            printf("Ok.\n");
            inBootloader = 1;
        } else {
            printf("Error: %s.\n", modbus_strerror(errno));
        }
        sleep(1);    // wait 1 second
    }

    if (eepromFormatCmd) {
        printf("Send format EEPROM command...\n");
        if (modbus_write_register(mb, HOLD_REG_CMD_EEPROM_ERASE, 1) == 1) {
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
            printf("To flash FW on running device, run\n%s %s\n", argv[0], casualUsage);
        }
        return 0;
    }

    FILE *file = fopen(fileName, "rb");
    if (file == NULL) {
        fprintf(stderr, "Error while opening firmware file: %s\n", strerror(errno));
        return -1;
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
        if (modbus_write_registers(mb, INFO_BLOCK_REG_ADDRESS, INFO_BLOCK_SIZE / 2, &data[filePointer / 2]) == (INFO_BLOCK_SIZE / 2)) {
            printf(" OK\n"); fflush(stdout);
            filePointer += INFO_BLOCK_SIZE;
            break;
        }
        printf("\n"); fflush(stdout);
        fprintf(stderr, "Error while sending info block: %s\n", modbus_strerror(errno));
        if (errno == EMBXSFAIL) {
            fprintf(stderr, "Data format is invalid or firmware signature doesn't match the device\n");
            return -1;
        } else if ((errno == EMBXILADD) || 
                   (errno == EMBXILVAL))  // some of our fws report illegal data value on nonexistent register
        {
            fprintf(stderr, "Not in bootloader mode? Try repeating with -j\n");
            return -1;
        }
        fflush(stderr);
        sleep(3);
        errorCount++;
        if (errorCount == MAX_ERROR_COUNT) {
            fprintf(stderr, "Error while sending info block.\n");
            fprintf(stderr, "Check connection, jump to bootloader and try again.\n");
            fflush(stderr);
            return -1;
        }
    }

    response_timeout.tv_sec = 1;
    response_timeout.tv_usec = 0;
    #if LIBMODBUS_VERSION_CHECK(3, 1, 2)
        modbus_set_response_timeout(mb, response_timeout.tv_sec, response_timeout.tv_usec);
    #else
        modbus_set_response_timeout(mb, &response_timeout);
    #endif

    printf("\n");
    while (filePointer < filesize) {
        fflush(stdout);
        printf("\rSending data block %u of %u...",
               (filePointer - INFO_BLOCK_SIZE) / DATA_BLOCK_SIZE + 1,
               (filesize - INFO_BLOCK_SIZE) / DATA_BLOCK_SIZE); fflush(stdout);
        if (modbus_write_registers(mb, DATA_BLOCK_REG_ADDRESS, DATA_BLOCK_SIZE / 2, &data[filePointer / 2]) == (DATA_BLOCK_SIZE / 2)) {
            filePointer += DATA_BLOCK_SIZE;
            errorCount = 0;
        } else {
            printf("\n"); fflush(stdout);
            fprintf(stderr, "Error while sending data block: %s\n", modbus_strerror(errno));
            if (errno == EMBXSFAIL) {
                fprintf(stderr, "Firmware file is corrupted?\n");
                return -1;
            }
            fflush(stderr);
            if (errorCount == MAX_ERROR_COUNT) {
                filePointer += DATA_BLOCK_SIZE;
            }
            if (errorCount >= MAX_ERROR_COUNT * 2) {
                return -1;
            }
            errorCount++;
        }
    }

    printf(" OK.\n\nAll done!\n");

    modbus_close(mb);
    modbus_free(mb);

    fclose(file);
    free(data);

    return 0;
}

int ensureIntIn(int param, const int array[], unsigned int arrayLen) {
    int valueIsIn = 0;
    for (unsigned int i = 0; i < arrayLen; i += 1){
        if (param == array[i]){
            valueIsIn = 1;
            break;
        }
    }
    return valueIsIn;
}


int ensureCharIn(char param, const char array[], unsigned int arrayLen) {
    int valueIsIn = 0;
    for (unsigned int i = 0; i < arrayLen; i += 1){
        if (param == array[i]){
            valueIsIn = 1;
            break;
        }
    }
    return valueIsIn;
}
