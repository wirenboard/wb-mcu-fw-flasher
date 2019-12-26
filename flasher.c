#include <stdio.h>
#include <stdlib.h>
#include <modbus/modbus.h>
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

int main(int argc, char *argv[])
{
    if (argc == 1) {
        printf("Welcome to Wiren Board flash tool.\n\n");
        printf("Usage:\n\n");

        printf("Param  Description                                               Default value\n\n");
        printf("-d     Communication device                                      -\n");
        printf("-f     Firmware file                                             -\n");
        printf("-a     Modbus ID                                                 1\n");
        printf("-j     Send jump to bootloader command                           -\n");
        printf("-u     Reset UART setting and MODBUS address to factory default  -\n");
        printf("-e     Format EEPROM (except device signature)                   -\n");
        printf("-r     Jump to bootloader register address                       129\n");
        printf("-D     Debug mode                                                -\n");

        printf("\nMinimal example: ./flasher -d /dev/ttyUSB0 -f firmware.wbfw\n\n");

        return 0;
    }

    // Default values
    char *device   = NULL;
    char *fileName = NULL;
    int   modbusID = 1;
    int   jumpCmd  = 0;
    int   uartResetCmd = 0;
    int   eepromFormatCmd = 0;
    int   jumpReg  = HOLD_REG_JUMP_TO_BOOTLOADER;
    int   debug    = 0;
    int   baudrate = 9600;

    int c;
    while ((c = getopt(argc, argv, "d:f:a:b:juer:D")) != -1) {
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
        case 'b':
            sscanf(optarg, "%d", &baudrate);
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
        default:
            printf("Parameters error.\n");
            break;
        }
    }

    modbus_t *mb = modbus_new_rtu(device, baudrate, 'N', 8, 2);

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

    modbus_set_slave(mb, modbusID);
    modbus_set_debug(mb, debug);

    struct timeval response_timeout;
    response_timeout.tv_sec = 0;
    response_timeout.tv_usec = 1000 * 1000;
    #if LIBMODBUS_VERSION_CHECK(3, 1, 2)
        modbus_set_response_timeout(mb, response_timeout.tv_sec, response_timeout.tv_usec);
    #else
        modbus_set_response_timeout(mb, &response_timeout);
    #endif

    if (jumpCmd) {
        printf("Send jump to bootloader command and wait 2 seconds...\n");
        if (modbus_write_register(mb, jumpReg, 1) == 1) {
            printf("Ok, device will jump to bootloader.\n");
        } else {
            printf("Error: %s.\n", modbus_strerror(errno));
            printf("May be device already in bootloader, try to send firmware...\n");
        }
        sleep(2);    // wait 2 seconds
    }

    if (uartResetCmd) {
        printf("Send reset UART settings and modbus address command...\n");
        if (modbus_write_register(mb, HOLD_REG_CMD_UART_SETTINGS_RESET, 1) == 1) {
            printf("Ok.\n");
        } else {
            printf("Error: %s.\n", modbus_strerror(errno));
        }
        sleep(1);    // wait 1 second
    }

    if (eepromFormatCmd) {
        printf("Send format EEPROM command...\n");
        if (modbus_write_register(mb, HOLD_REG_CMD_EEPROM_ERASE, 1) == 1) {
            printf("Ok.\n");
        } else {
            printf("Error: %s.\n", modbus_strerror(errno));
        }
        sleep(1);    // wait 1 second
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
            printf(" OK\n");
            filePointer += INFO_BLOCK_SIZE;
            break;
        }
        printf("\nError while sending info block: %s", modbus_strerror(errno));
        sleep(3);
        errorCount++;
        if (errorCount == MAX_ERROR_COUNT) {
            printf("\nError while sending info block.\n");
            printf("Check connection, jump to bootloader and try again.\n");
            return -1;
        }
    }

    printf("\n");
    while (filePointer < filesize) {
        fflush(stdout);
        printf("\rSending data block %u of %u...",
               (filePointer - INFO_BLOCK_SIZE) / DATA_BLOCK_SIZE + 1,
               (filesize - INFO_BLOCK_SIZE) / DATA_BLOCK_SIZE);
        if (modbus_write_registers(mb, DATA_BLOCK_REG_ADDRESS, DATA_BLOCK_SIZE / 2, &data[filePointer / 2]) == (DATA_BLOCK_SIZE / 2)) {
            filePointer += DATA_BLOCK_SIZE;
            errorCount = 0;
        } else {
            printf("\nError while sending data block: %s\n", modbus_strerror(errno));
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
