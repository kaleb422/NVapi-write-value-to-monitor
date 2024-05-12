#pragma comment(lib, "nvapi64.lib")

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <tchar.h>
#include "nvapi.h"
#include "targetver.h"


BOOL WriteValueToMonitor(NvPhysicalGpuHandle hPhysicalGpu, NvU32 displayId, BYTE input_value, BYTE command_code, BYTE register_address);


int main(int argc, char* argv[]) {

    int display_index = 0;
    BYTE input_value = 0;
    BYTE command_code = 0;  //VCP code or equivalent
    BYTE register_address = 0x51;

    // Usage: writeValueToMonitor.exe [display_index] [input_value] [command_code]
    // Uses default register addres 0x51 used for VCP codes
    if (argc == 4) {
        display_index = atoi(argv[1]);
        input_value = (BYTE)strtol(argv[2], NULL, 16);
        command_code = (BYTE)strtol(argv[3], NULL, 16);
    }

    // Usage: writeValueToMonitor.exe [display_index] [input_value] [command_code] [register_address]
    // Uses default register addres 0x51 used for VCP codes
    else if (argc == 5) {
        display_index = atoi(argv[1]);
        input_value = (BYTE)strtol(argv[2], NULL, 16);
        command_code = (BYTE)strtol(argv[3], NULL, 16);
        register_address = (BYTE)strtol(argv[4], NULL, 16);
    }
    else {
        printf("Incorrect Number of arguments!\n\n");

        printf("Arguments:\n");
        printf("display_index   - Index assigned to monitor (0 for first screen)\n");
        printf("input_value     - value to right to screen\n");
        printf("command_code    - VCP code or other\n");
        printf("register_address - Adress to write to, default 0x51 for VCP codes\n\n");

        printf("Usage:\n");
        printf("writeValueToScreen.exe [display_index] [input_value] [command_code]\n");
        printf("OR\n");
        printf("writeValueToScreen.exe [display_index] [input_value] [command_code] [register_address]\n");
        return 1;
    }
    

    //printf("%d, %ld, %d, %d", display_index, input_value, command_code, register_address);

    NvAPI_Status nvapiStatus = NVAPI_OK;

    // Initialize NVAPI.
    if ((nvapiStatus = NvAPI_Initialize()) != NVAPI_OK)
    {
        printf("NvAPI_Initialize() failed with status %d\n", nvapiStatus);
        return 1;
    }


    //
    // Enumerate display handles
    //
    NvDisplayHandle hDisplay_a[NVAPI_MAX_PHYSICAL_GPUS * NVAPI_MAX_DISPLAY_HEADS] = { 0 };
    for (unsigned int i = 0; nvapiStatus == NVAPI_OK; i++)
    {
        nvapiStatus = NvAPI_EnumNvidiaDisplayHandle(i, &hDisplay_a[i]);
        
        if (nvapiStatus != NVAPI_OK && nvapiStatus != NVAPI_END_ENUMERATION)
        {
            printf("NvAPI_EnumNvidiaDisplayHandle() failed with status %d\n", nvapiStatus);
            return 1;
        }
    }

   
    // Get GPU id assiciated with display ID
    NvPhysicalGpuHandle hGpu = NULL;
    NvU32 pGpuCount = 0;
    nvapiStatus = NvAPI_GetPhysicalGPUsFromDisplay(hDisplay_a[display_index], &hGpu, &pGpuCount);
    if (nvapiStatus != NVAPI_OK)
    {
        printf("NvAPI_GetPhysicalGPUFromDisplay() failed with status %d\n", nvapiStatus);
        return 1;
    }

    // Get the display id for subsequent I2C calls via NVAPI:
    NvU32 outputID = 0;
    nvapiStatus = NvAPI_GetAssociatedDisplayOutputId(hDisplay_a[display_index], &outputID);
    if (nvapiStatus != NVAPI_OK)
    {
        printf("NvAPI_GetAssociatedDisplayOutputId() failed with status %d\n", nvapiStatus);
        return 1;
    }


    BOOL result = WriteValueToMonitor(hGpu, outputID, input_value, command_code, register_address);
    if (!result)
    {
        printf("Changing input failed\n");
    }
    printf("\n");


}


// This function calculates the (XOR) checksum of the I2C register
void CalculateI2cChecksum(const NV_I2C_INFO& i2cInfo)
{
    // Calculate the i2c packet checksum and place the 
    // value into the packet

    // i2c checksum is the result of xor'ing all the bytes in the 
    // packet (except for the last data byte, which is the checksum 
    // itself)

    // Packet consists of:

    // The device address...
    BYTE checksum = i2cInfo.i2cDevAddress;

    // Register address...
    for (unsigned int i = 0; i < i2cInfo.regAddrSize; ++i)
    {
        checksum ^= i2cInfo.pbI2cRegAddress[i];
    }

    // And data bytes less last byte for checksum...
    for (unsigned int i = 0; i < i2cInfo.cbSize - 1; ++i)
    {
        checksum ^= i2cInfo.pbData[i];
    }

    // Store calculated checksum in the last byte of i2c packet
    i2cInfo.pbData[i2cInfo.cbSize - 1] = checksum;
}

// This macro initializes the i2cinfo structure
#define  INIT_I2CINFO(i2cInfo, i2cVersion, displayId, isDDCPort,   \
        i2cDevAddr, regAddr, regSize, dataBuf, bufSize, speed)     \
do {                                                               \
    i2cInfo.version         = i2cVersion;                          \
    i2cInfo.displayMask     = displayId;                           \
    i2cInfo.bIsDDCPort      = isDDCPort;                           \
    i2cInfo.i2cDevAddress   = i2cDevAddr;                          \
    i2cInfo.pbI2cRegAddress = (BYTE*) &regAddr;                    \
    i2cInfo.regAddrSize     = regSize;                             \
    i2cInfo.pbData          = (BYTE*) &dataBuf;                    \
    i2cInfo.cbSize          = bufSize;                             \
    i2cInfo.i2cSpeed        = speed;                               \
}while (0)

// This function writes the input_value to the display over the I2C bus by issuing commands and data
BOOL WriteValueToMonitor(NvPhysicalGpuHandle hPhysicalGpu, NvU32 displayId,BYTE input_value, BYTE command_code, BYTE register_address)
{
    NvAPI_Status nvapiStatus = NVAPI_OK;

    NV_I2C_INFO i2cInfo = { 0 };
    i2cInfo.version = NV_I2C_INFO_VER;
    //
    // The 7-bit I2C address for display = Ox37
    // Since we always use 8bits to address, this 7-bit addr (0x37) is placed on
    // the upper 7 bits, and the LSB contains the Read/Write flag:
    // Write = 0 and Read =1;
    //
    NvU8 i2cDeviceAddr = 0x37;
    NvU8 i2cWriteDeviceAddr = i2cDeviceAddr << 1; //0x6E
    NvU8 i2cReadDeviceAddr = i2cWriteDeviceAddr | 1; //0x6F


    //
    // Now Send a write packet to modify current brightness value to 20 (0x14)
    // The packet consists of the following bytes
    // 0x6E - i2cWriteDeviceAddr
    // Ox?? - register_address
    // 0x84 - 0x80 OR n where n = 4 bytes for "modify a value" request
    // 0x03 - change a value flag
    // 0x?? - command_code
    // Ox00 - input_value high byte
    // 0x?? - input_value low byte
    // 0x?? - checksum, , xor'ing all the above bytes
    //
    BYTE registerAddr[] = { register_address };
    BYTE modifyBytes[] = { 0x84, 0x03, command_code, 0x00, input_value, 0xDD };

    INIT_I2CINFO(i2cInfo, NV_I2C_INFO_VER, displayId, TRUE, i2cWriteDeviceAddr,
        registerAddr, sizeof(registerAddr), modifyBytes, sizeof(modifyBytes), 27);
    CalculateI2cChecksum(i2cInfo);

    nvapiStatus = NvAPI_I2CWrite(hPhysicalGpu, &i2cInfo);
    if (nvapiStatus != NVAPI_OK)
    {
        printf("  NvAPI_I2CWrite (revise brightness) failed with status %d\n", nvapiStatus);
        return FALSE;
    }

    return TRUE;
}
