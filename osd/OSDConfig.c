#include <stdio.h>
#include <kernel.h>
#include <string.h>
#include <libcdvd.h>
#include "libcdvd_add.h"
#include <osd_config.h>

#include "OSDConfig.h"

/*	Parsing of values from the EEPROM and setting them into the EE kernel
	was done in different ways, across different browser versions.

	The early browsers of ROM v1.00 and v1.01 (SCPH-10000/SCPH-15000)
	parsed the values within the EEPROM into global variables,
	which are used to set the individual fields in the OSD configuration.

	The newer browsers parsed the values into a bitfield structure,
	which does not have the same layout as the OSD configuration structure.

	Both designs had the parsing and the preparation of the OSD
	configuration data separated, presumably for clarity of code and
	to achieve low-coupling (perhaps they belonged to different modules).

	But for efficiency, the parsing and preparation of the OSD
	configuration data here is done together. */

static short int ConsoleRegion=-1,ConsoleOSDRegion=-1,ConsoleOSDLanguage=-1;
static short int ConsoleOSDRegionInitStatus=0, ConsoleRegionParamInitStatus=0;	//0 = Not init. 1 = Init complete. <0 = Init failed.
static unsigned char ConsoleRegionData[16] = {0, 0, 0xFF, 0xFF, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

extern int (*_ps2sdk_close)(int) __attribute__((section("data")));
extern int (*_ps2sdk_open)(const char*, int) __attribute__((section("data")));
extern int (*_ps2sdk_read)(int, void*, int) __attribute__((section("data")));

static int GetConsoleRegion(void);
static int GetOSDRegion(void);
static void InitOSDDefaultLanguage(int region, const char *language);
static int IsLanguageValid(int region, int language);

//static char SystemDataFolder[]="BRDATA-SYSTEM";	//Not in use.
static char SystemExecFolder[]="BREXEC-SYSTEM";
//static char DVDPLExecFolder[]="BREXEC-DVDPLAYER";	//Not in use

static int InitMGRegion(void){
	int result, status;

	/*	There's a nice RPC function provided by XCDVDFSV and XCDVDMAN, but the protokernel consoles lack them.
			Even if they don't need this command to be run at startup, the difference in alignment structures in the CDVDFSV versions makes mixing the modules and EE client libraries a bad idea. */

	if(ConsoleRegionParamInitStatus==0){
		do{
#ifndef FMCB_PSX_SUPPORT
			if((result=sceCdAltReadRegionParams(ConsoleRegionData, &status))==0) ConsoleRegionParamInitStatus=1;
#else
			if((result=sceCdReadRegionParams(ConsoleRegionData, &status))==0) ConsoleRegionParamInitStatus=1;
#endif
			else{
				if(status&0x100){
					//MECHACON does not support this function.
					ConsoleRegionParamInitStatus=-1;
					break;
				}
				else ConsoleRegionParamInitStatus=1;
			}
		}while((result==0) || (status&0x80));
	}

	return ConsoleRegionParamInitStatus;
}

void OSDUpdateSystemPaths(void)
{
	int region;
	char regions[CONSOLE_REGION_COUNT]={'I', 'A', 'E', 'C'};

	region = OSDInitConsoleRegion();
	if(region>=0 && region<CONSOLE_REGION_COUNT)
	{
	//	SystemDataFolder[1]=regions[region];
		SystemExecFolder[1]=regions[region];
	//	DVDPLExecFolder[1]=regions[region];
	}
}

int OSDGetMGRegion(void)
{
	return((InitMGRegion()>=0)?ConsoleRegionData[8]:0);
}

//Not in use.
/* static int ConsoleInitRegion(void)
{
	GetOSDRegion();
	return InitMGRegion();
}

static int ConsoleRegionParamsInitPS1DRV(const char *romver)
{
	int result;

	if(ConsoleInitRegion() >= 0)
	{
		ConsoleRegionData[2] = romver[4];
		result = 1;
	} else
		result = 0;

	return result;
} */

static int GetConsoleRegion(void)
{
	char romver[16];
	int fd, result;

	if((result=ConsoleRegion)<0)
	{
		if((fd = _ps2sdk_open("rom0:ROMVER", O_RDONLY)) >= 0)
		{
			_ps2sdk_read(fd, romver, sizeof(romver));
			_ps2sdk_close(fd);
		//	ConsoleRegionParamsInitPS1DRV(romver);

			switch(romver[4])
			{
				case 'C':
					ConsoleRegion=CONSOLE_REGION_CHINA;
					break;
				case 'E':
					ConsoleRegion=CONSOLE_REGION_EUROPE;
					break;
				case 'H':
				case 'A':
					ConsoleRegion=CONSOLE_REGION_USA;
					break;
				case 'J':
					ConsoleRegion=CONSOLE_REGION_JAPAN;
			}

			result=ConsoleRegion;
		} else
			result = -1;
	}

	return result;
}

static int CdReadOSDRegionParams(char *OSDVer)
{
	int result;

	if(OSDVer[4] == '?')
	{
		if(InitMGRegion() >= 0)
		{
			result = 1;
			OSDVer[4] = ConsoleRegionData[3];
			OSDVer[5] = ConsoleRegionData[4];
			OSDVer[6] = ConsoleRegionData[5];
			OSDVer[7] = ConsoleRegionData[6];
		} else
			result = 0;
	} else {
		ConsoleRegionParamInitStatus = -256;
		result = 0;
	}

	return result;
}

static int GetOSDRegion(void)
{
	char OSDVer[16];
	int fd;

	if(ConsoleOSDRegionInitStatus == 0 || ConsoleOSDRegion == -1)
	{
		ConsoleOSDRegionInitStatus = 1;
		if((fd = _ps2sdk_open("rom0:OSDVER", O_RDONLY)) >= 0)
		{
			_ps2sdk_read(fd, OSDVer, sizeof(OSDVer));
			_ps2sdk_close(fd);
			CdReadOSDRegionParams(OSDVer);
			switch(OSDVer[4] - 'A')
			{
				case 'A':
					ConsoleOSDRegion = OSD_REGION_USA;
					break;
				case 'C':
					ConsoleOSDRegion = OSD_REGION_CHINA;
					break;
				case 'E':
					ConsoleOSDRegion = OSD_REGION_EUROPE;
					break;
				case 'H':
					ConsoleOSDRegion = OSD_REGION_ASIA;
					break;
				case 'J':
					ConsoleOSDRegion = OSD_REGION_JAPAN;
					break;
				case 'K':
					ConsoleOSDRegion = OSD_REGION_KOREA;
					break;
				case 'R':
					ConsoleOSDRegion = OSD_REGION_RUSSIA;
					break;
				default:
					ConsoleOSDRegion = OSD_REGION_INVALID;
			}

			if(ConsoleOSDRegion != OSD_REGION_INVALID)
				InitOSDDefaultLanguage(ConsoleOSDRegion, &OSDVer[5]);
		} else
			ConsoleOSDRegion = OSD_REGION_INVALID;
	}

	return ConsoleOSDRegion;
}

static void InitOSDDefaultLanguage(int region, const char *language)
{
	int DefaultLang;

	DefaultLang = -1;
	if(ConsoleOSDLanguage == -1)
	{
		if(language != NULL)
		{
			if(strncmp(language, "jpn", 3) == 0)
				DefaultLang = LANGUAGE_JAPANESE;
			else if(strncmp(language, "eng", 3) == 0)
				DefaultLang = LANGUAGE_ENGLISH;
			else if(strncmp(language, "fre", 3) == 0)
				DefaultLang = LANGUAGE_FRENCH;
			else if(strncmp(language, "spa", 3) == 0)
				DefaultLang = LANGUAGE_SPANISH;
			else if(strncmp(language, "ger", 3) == 0)
				DefaultLang = LANGUAGE_GERMAN;
			else if(strncmp(language, "ita", 3) == 0)
				DefaultLang = LANGUAGE_ITALIAN;
			else if(strncmp(language, "dut", 3) == 0)
				DefaultLang = LANGUAGE_DUTCH;
			else if(strncmp(language, "por", 3) == 0)
				DefaultLang = LANGUAGE_PORTUGUESE;
			else if(strncmp(language, "rus", 3) == 0)
				DefaultLang = LANGUAGE_RUSSIAN;
			else if(strncmp(language, "kor", 3) == 0)
				DefaultLang = LANGUAGE_KOREAN;
			else if(strncmp(language, "tch", 3) == 0)
				DefaultLang = LANGUAGE_TRAD_CHINESE;
			else if(strncmp(language, "sch", 3) == 0)
				DefaultLang = LANGUAGE_SIMPL_CHINESE;
			else DefaultLang = -1;
		}

		//Check if the specified language is valid for the region
		if(!IsLanguageValid(region, DefaultLang))
		{
				switch(region)
				{
					case OSD_REGION_JAPAN:
						DefaultLang = LANGUAGE_JAPANESE;
						break;
					case OSD_REGION_CHINA:
						DefaultLang = LANGUAGE_SIMPL_CHINESE;
						break;
					case OSD_REGION_RUSSIA:
						DefaultLang = LANGUAGE_RUSSIAN;
						break;
					case OSD_REGION_KOREA:
						DefaultLang = LANGUAGE_KOREAN;
						break;
					case OSD_REGION_USA:
					case OSD_REGION_EUROPE:
					case OSD_REGION_ASIA:
					default:
						DefaultLang = LANGUAGE_ENGLISH;
				}
		}

		ConsoleOSDLanguage = DefaultLang;
	}
}

static int IsLanguageValid(int region, int language)
{
	switch(region)
	{
		case OSD_REGION_JAPAN:
			return(language == LANGUAGE_JAPANESE || language == LANGUAGE_ENGLISH) ? language : -1;
		case OSD_REGION_CHINA:
			return(language == LANGUAGE_ENGLISH || language == LANGUAGE_SIMPL_CHINESE) ? language : -1;
		case OSD_REGION_RUSSIA:
			return(language == LANGUAGE_ENGLISH || language == LANGUAGE_RUSSIAN) ? language : -1;
		case OSD_REGION_KOREA:
			return(language == LANGUAGE_ENGLISH || language == LANGUAGE_KOREAN) ? language : -1;
		case OSD_REGION_ASIA:
			return(language == LANGUAGE_ENGLISH || language == LANGUAGE_TRAD_CHINESE) ? language : -1;
		case OSD_REGION_USA:
		case OSD_REGION_EUROPE:
		default:
			return(language <= LANGUAGE_PORTUGUESE && region > OSD_REGION_JAPAN) ? language : -1;
	}
}

int OSDInitConsoleRegion(void)
{	//Default to Japan, if the region cannot be obtained.
	int result;

	result=GetConsoleRegion();

	return(result<0?0:result);
}

static int InitOSDRegionParams(void)
{
	int region;

	if((region = GetOSDRegion()) < 0)
		InitOSDDefaultLanguage(region = OSDInitConsoleRegion(), NULL);

	return region;
}

//Old system, for version = 1
/* static int _GetLanguage(const unsigned char* OSDConfigBuffer){
	int result;
	unsigned int LanguageSetting;

	//Use the appropriate fields from the NVRAM, depending on whether the console is a domestic (Japanese) or export set.
	LanguageSetting=(OSDConfigBuffer[0xF]>>5==0)?OSDConfigBuffer[0xF]>>4&0x1:OSDConfigBuffer[0x10]&0x1F;

	if(OSDInitConsoleRegion()==0){
		result=(LanguageSetting^1)?LANGUAGE_JAPANESE:LANGUAGE_ENGLISH;
	}
	else result=(LanguageSetting>0 && LanguageSetting<8)?LanguageSetting:LANGUAGE_ENGLISH;

	return result;
}	*/

static int GetOSDLanguage(void)
{
	if(ConsoleOSDLanguage == -1)
		InitOSDRegionParams();

	return ConsoleOSDLanguage;
}

static inline int _GetLanguage(const unsigned char* OSDConfigBuffer)
{
	int region, DefaultLanguage, LanguageSetting;

	//Use the appropriate fields from the NVRAM, depending on whether the console is a domestic (Japanese) or export set.
	LanguageSetting=(OSDConfigBuffer[0xF]>>5==0)?OSDConfigBuffer[0xF]>>4&0x1:OSDConfigBuffer[0x10]&0x1F;

	region = InitOSDRegionParams();
	DefaultLanguage = GetOSDLanguage();
	if(region != 0)
	{
		if(LanguageSetting == LANGUAGE_JAPANESE)
			return DefaultLanguage;
	}

	return(IsLanguageValid(region, LanguageSetting) ? LanguageSetting : DefaultLanguage);
}

/*	Notes:
		SCPH-30000 (boot ROM v1.50):
			config->japLanguage=1;
			config->ps1drvConfig=OSDConfigBuffer[0]&0x11
			config->region=1
		SCPH-39006 (v8, boot ROM v1.60):
			config->japLanguage=1;
			config->ps1drvConfig=OSDConfigBuffer[0]&0x11
			config->region=1
		SCPH-10000 (boot ROM v1.01):
			config->japLanguage=OSDConfigBuffer[0xF]>>4&1
			config->ps1drvConfig=OSDConfigBuffer[0]
			config->region (Not set)
		PX300-1 (PAL PS2TV, boot ROM v2.50):
			config->japLanguage=1;
			config->ps1drvConfig=OSDConfigBuffer[0]&0x11
			config->region=2
		HDDOSD (USA, Browser update 2.00):
			config->japLanguage=1;
			config->ps1drvConfig=OSDConfigBuffer[0]&0x11
			config->region=1
		DTL-H30001 (boot ROM v1.10):
			config->japLanguage=1;
			config->ps1drvConfig=OSDConfigBuffer[0]&0x11
			config->region=1
		SCPH-50000 (boot ROM v1.70):
			config->japLanguage=1;
			config->ps1drvConfig=OSDConfigBuffer[0]&0x11
			config->region=1
		SCPH-50009 (boot ROM v1.90):
			config->japLanguage=1;
			config->ps1drvConfig=OSDConfigBuffer[0]&0x11
			config->region=2
		SCPH-90010 (boot ROM v2.30):
			config->japLanguage=1;
			config->ps1drvConfig=OSDConfigBuffer[0]&0x11
			config->version=2

		Version = 0 (Protokernel consoles only) NTSC-J, 1 = ROM versions up to v1.70, 2 = v1.80 and later. 2 = support for extended languages (Osd2 bytes 3 and 4)
					In the homebrew PS2SDK, this was known as the "region".
		japLanguage = 0 (Japanese, protokernel consoles only), 1 = non-Japanese (Protokernel consoles only). Newer browsers have this set always to 1.
*/
static inline int ParseOSDConfig(const unsigned char* OSDConfigBuffer, ConfigParam* config, Config2Param* config2){
	int language;

	config->spdifMode=OSDConfigBuffer[0xF]&1;
	config->screenType=OSDConfigBuffer[0xF]>>1&3;
	config->videoOutput=OSDConfigBuffer[0xF]>>3&1;
	config->japLanguage=1;
	config->ps1drvConfig=OSDConfigBuffer[0]&0x11;

	language = _GetLanguage(OSDConfigBuffer);
	config->language = language > LANGUAGE_PORTUGUESE ? LANGUAGE_ENGLISH : language;
	//0 = early OSD, 1 = newer OSD, 2 = newer OSD with support for the extended languages (Osd2 bytes 3 and 4)
	config->version=2;
	config->timezoneOffset=OSDConfigBuffer[0x12]|((u32)OSDConfigBuffer[0x11]&0x7)<<8;

	config2->daylightSaving=OSDConfigBuffer[0x11]>>3&1;
	config2->timeFormat=OSDConfigBuffer[0x11]>>4&1;
	config2->dateFormat=OSDConfigBuffer[0x11]>>5&3;
	config2->version=2;
	config2->language=language;

	return(OSDConfigBuffer[0x11]>>7^1);	//Return whether the "OSD Init" bit is set (Not set = not initialized).
}

static int ReadConfigFromNVM(ConfigParam* config, Config2Param* config2){
	int OpResult, result;
	unsigned char OSDConfigBuffer[30];

	do{
		sceCdOpenConfig(1, 0, 2, &OpResult);
	}while(OpResult&9);

	do{
		result=sceCdReadConfig(OSDConfigBuffer, &OpResult);
	}while(OpResult&9 || result==0);

	do{
		result=sceCdCloseConfig(&OpResult);
	}while(OpResult&9 || result==0);

	return ParseOSDConfig(OSDConfigBuffer, config, config2);
}

int OSDLoadConfigFromNVM(ConfigParam *pConfig, Config2Param *pConfig2){
	int result;
	ConfigParam config;
	Config2Param config2;

	if((result=ReadConfigFromNVM(&config, &config2))!=0){
//		printf("Warning! Settings in NVRAM are not configured.\n");
	}

	*pConfig = config;
	*pConfig2 = config2;

	SetOsdConfigParam(&config);
	GetOsdConfigParam(&config);
	//Unpatched Protokernels cannot retain values set in the region field, and don't support SetOsdConfigParam2().
	if(config.version) SetOsdConfigParam2(&config2, 4, 0);
	SetGsVParam(config.videoOutput?1:0);

	return result;
}

//Not in use
/* const char *OSDGetSystemDataFolder(void)
{
	return SystemDataFolder;
} */

const char *OSDGetSystemExecFolder(void)
{
	return SystemExecFolder;
}

//Not in use
/* const char *OSDGetDVDPLExecFolder(void)
{
	return DVDPLExecFolder;
} */
