enum CONSOLE_REGIONS {
    CONSOLE_REGION_INVALID = -1,
    CONSOLE_REGION_JAPAN = 0,
    CONSOLE_REGION_USA, // USA and HK/SG.
    CONSOLE_REGION_EUROPE,
    CONSOLE_REGION_CHINA,

    CONSOLE_REGION_COUNT
};

enum OSD_REGIONS {
    OSD_REGION_INVALID = -1,
    OSD_REGION_JAPAN = 0,
    OSD_REGION_USA,
    OSD_REGION_EUROPE,
    OSD_REGION_CHINA,
    OSD_REGION_RUSSIA,
    OSD_REGION_KOREA,
    OSD_REGION_ASIA,

    OSD_REGION_COUNT
};

int OSDInitConsoleRegion(void);
int OSDLoadConfigFromNVM(ConfigParam *pConfig, Config2Param *pConfig2);
void OSDUpdateSystemPaths(void);
int OSDGetMGRegion(void);
const char *OSDGetSystemExecFolder(void);
const char *OSDGetSystemDataFolder(void);
const char *OSDGetDVDPLExecFolder(void);
