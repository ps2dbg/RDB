#include <kernel.h>
#include <sifrpc.h>
#include <string.h>
#include <libcdvd.h>

#include "libcdvd_add.h"

static unsigned char MECHACON_CMD_S36_supported = 0;

// Initialize add-on functions. Currently only retrieves the MECHACON's version to determine what sceCdAltGetRegionParams() should do.
int InitLibcdvd_addOnFunctions(void)
{
    int result, status, i;
    unsigned char MECHA_version_data[3];
    unsigned int MECHA_version;

    // Like how CDVDMAN checks sceCdMV(), do not continuously attempt to get the MECHACON version because some consoles (e.g. DTL-H301xx) can't return one.
    for (i = 0; i <= 100; i++) {
        if ((result = sceCdMV(MECHA_version_data, &status)) != 0 && ((status & 0x80) == 0)) {
            MECHA_version = MECHA_version_data[2] | ((unsigned int)MECHA_version_data[1] << 8) | ((unsigned int)MECHA_version_data[0] << 16);
            MECHACON_CMD_S36_supported = (0x5FFFF < MECHA_version);
            return 0;
        }
    }

    //	printf("Failed to get MECHACON version: %d 0x%x\n", result, status);

    return -1;
}

/*
     This function provides an equivalent of the sceCdGetRegionParams function from the newer CDVDMAN modules. The old CDVDFSV and CDVDMAN modules don't support this S-command.
    It's supported by only slimline consoles, and returns regional information (e.g. MECHACON version, MG region mask, DVD player region letter etc.).
*/
int sceCdAltReadRegionParams(unsigned char *data, int *status)
{
    unsigned char RegionData[15];
    int result;

    memset(data, 0, 13);
    if (MECHACON_CMD_S36_supported) {
        if ((result = sceCdApplySCmd(0x36, NULL, 0, RegionData, sizeof(RegionData))) != 0) {
            *status = RegionData[0];
            memcpy(data, &RegionData[1], 13);
        }
    } else {
        *status = 0x100;
        result = 1;
    }

    return result;
}

// This function provides an equivalent of the sceCdBootCertify function from the newer CDVDMAN modules. The old CDVDFSV and CDVDMAN modules don't support this S-command.
int sceCdAltBootCertify(const unsigned char *data)
{
    int result;
    unsigned char CmdResult;

    if ((result = sceCdApplySCmd(0x1A, data, 4, &CmdResult, 1)) != 0) {
        result = CmdResult;
    }

    return result;
}
