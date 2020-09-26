#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include "common.h"
#include <xpwn/libxpwn.h>
#include <xpwn/nor_files.h>
#include <dmg/dmg.h>
#include <dmg/filevault.h>
#include <xpwn/ibootim.h>
#include <xpwn/plist.h>
#include <xpwn/outputstate.h>
#include <hfs/hfslib.h>
#include <dmg/dmglib.h>
#include <xpwn/pwnutil.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>


// cherry's utils
#include "utils/ramdiskI_n90.h"
#include "utils/partition_n90.h"
#include "utils/bin.h"
#include "utils/bin4.h"
#include "iBoot32Patcher/iBoot32Patcher.c"
#include "utils/scab.h"

#define VERSION     1
#define NIVERSION   4
#define NINIVERSION 2

#define FIXNUM      147

#ifdef WIN32
#include <windows.h>
#endif

char endianness;

static char* tmpFile = NULL;

static AbstractFile* openRoot(void** buffer, size_t* rootSize) {
	static char tmpFileBuffer[512];

	if((*buffer) != NULL) {
		return createAbstractFileFromMemoryFile(buffer, rootSize);
	} else {
		if(tmpFile == NULL) {
#ifdef WIN32
			char tmpFilePath[512];
			GetTempPath(512, tmpFilePath);
			GetTempFileName(tmpFilePath, "root", 0, tmpFileBuffer);
			CloseHandle(CreateFile(tmpFilePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_DELETE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, NULL));
#else
			strcpy(tmpFileBuffer, "/tmp/rootXXXXXX");
			close(mkstemp(tmpFileBuffer));
			FILE* tFile = fopen(tmpFileBuffer, "wb");
			fclose(tFile);
#endif
			tmpFile = tmpFileBuffer;
		}
		return createAbstractFileFromFile(fopen(tmpFile, "r+b"));
	}
}

void closeRoot(void* buffer) {
	if(buffer != NULL) {
		free(buffer);
	}

	if(tmpFile != NULL) {
		unlink(tmpFile);
	}
}

int main(int argc, char* argv[]) {
	
    init_libxpwn(&argc, argv);
    
    Dictionary* info;
    Dictionary* info7;
    Dictionary* shsh;
    Dictionary* firmwarePatches;
    Dictionary* patchDict;
    ArrayValue* patchArray;
    
    Dictionary* firmwareInject;
    Dictionary* injectDict;
    StringValue* injectFileValue;
    StringValue* injectmanifestValue;
    Dictionary* baseFWPath;
    Dictionary* baseFWDict;
    StringValue* baseFWFileValue;
    
    void* buffer;
    
    StringValue* actionValue;
    StringValue* pathValue;
    
    StringValue* fileValue;
    
    StringValue* patchValue;
    char* patchPath;
    
    char* rootFSPathInIPSW;
    io_func* rootFS;
    Volume* rootVolume;
    size_t rootSize;
    size_t preferredRootSize = 0;
    size_t preferredRootSizeAdd = 0;
    size_t minimumRootSize = 0;
    
    char* ramdiskFSPathInIPSW;
    unsigned int ramdiskKey[32];
    unsigned int ramdiskIV[16];
    unsigned int* pRamdiskKey = NULL;
    unsigned int* pRamdiskIV = NULL;
    io_func* ramdiskFS;
    Volume* ramdiskVolume;
    size_t ramdiskGrow = 0;
    
    Dictionary* manifest = NULL;
    AbstractFile *manifestFile;
    char manifestDirty = FALSE;
    AbstractFile *otaFile = NULL;
    AbstractFile *APFile = NULL;
    char* updateRamdiskFSPathInIPSW = NULL;
    
    int i;
    
    OutputState* outputState;
    char* IPSW7Inject;
    OutputState* output7State;
    char* bundle7Path;
    
    int partitionSize=0;
    int tarInjectSize=0;
    int exploitDiskSize=0;
    
    int isIOS=0;
    
    AbstractFile* partitionFile = NULL;
    AbstractFile* tarInjectFile = NULL;
    AbstractFile* exploitDisk = NULL;
    
    //size_t origiBootLength;
    //char *origibot;
    
    char* bundlePath;
    int mergePaths;
    char* outputIPSW;
    
    void* imageBuffer;
    size_t imageSize;
    
    AbstractFile* bootloader39 = NULL;
    AbstractFile* bootloader46 = NULL;
    AbstractFile* applelogo = NULL;
    AbstractFile* recoverymode = NULL;
    
    char noWipe = FALSE;
    
    char useJailbreak = FALSE;
    char unlockBaseband = FALSE;
    char selfDestruct = FALSE;
    char use39 = FALSE;
    char use46 = FALSE;
    char doBootNeuter = FALSE;
    char flashNOR = TRUE;
    char doDeRebusAntiquis = FALSE;
    char updateBB = FALSE;
    char useMemory = FALSE;
    
    unsigned int key[32];
    unsigned int iv[16];
    
    unsigned int* pKey = NULL;
    unsigned int* pIV = NULL;
    
    /* build ver */
    int MajorVer = VERSION;
    int MinorVer = NIVERSION;
    int MinorMinorVer = NINIVERSION;
    
    int BNver = MinorVer+10;
    int MNuver = (MinorVer*3) + (MinorMinorVer*5) + (FIXNUM);
    
    XLOG(0, "** This build is fork by twitter-ID: @dora2_yururi\n");
    XLOG(0, "** ch3rry ver %d.%d.%d [Build: %d%X%d]\n",
         MajorVer,      // Major version
         MinorVer,
         MinorMinorVer,
         MajorVer,      // Major version
         BNver,         // Build version
         MNuver         // Minor version
         );
    
	if(argc < 3) {
		XLOG(0, "usage %s <input.ipsw> <target.ipsw> [-j Jailbreak] [-b <bootimage.png>] [-r <recoveryimage.png>] [-s <system partition size>] [-S <system partition add>] [-memory] [-bbupdate] [-tethered] [-derebusantiquis <ipsw>] [-a <apticket.xml>] [-ota BuildManifest] [-nowipe] [-e \"<action to exclude>\"] [-ramdiskgrow <blocks>] [[-unlock] [-use39] [-use46] [-cleanup] -3 <bootloader 3.9 file> -4 <bootloader 4.6 file>] <package1.tar> <package2.tar>...\n", argv[0]);
        
        XLOG(0, "\nch3rry's option:%s <input.ipsw> <target.ipsw> [-memory] [-bbupdate] [-derebusantiquis <ipsw>] [-a <apticket.xml>] <package1.tar> <package2.tar> ...\n", argv[0]);
        
		return 0;
	}

    outputIPSW = argv[2];
    
	int* toRemove = NULL;
	int numToRemove = 0;

	for(i = 3; i < argc; i++) {
		if(argv[i][0] != '-') {
			break;
		}

		if(strcmp(argv[i], "-memory") == 0) {
			useMemory = TRUE;
			continue;
		}
        
        if(strcmp(argv[i], "-j") == 0) {
            useJailbreak = TRUE;
            continue;
        }

		if(strcmp(argv[i], "-s") == 0) {
			int size;
			sscanf(argv[i + 1], "%d", &size);
			preferredRootSize = size;
			i++;
			continue;
		}

		if(strcmp(argv[i], "-S") == 0) {
			int size;
			sscanf(argv[i + 1], "%d", &size);
			preferredRootSizeAdd = size;
			i++;
			continue;
		}

		if(strcmp(argv[i], "-ramdiskgrow") == 0) {
			int size;
			sscanf(argv[i + 1], "%d", &size);
			ramdiskGrow = size;
			i++;
			continue;
		}

		if(strcmp(argv[i], "-nowipe") == 0) {
			noWipe = TRUE;
			continue;
		}

		if(strcmp(argv[i], "-bbupdate") == 0) {
			updateBB = TRUE;
			continue;
		}
        
        if(strcmp(argv[i], "-tethered") == 0) {
            flashNOR = FALSE;
            continue;
        }
        
        if(strcmp(argv[i], "-derebusantiquis") == 0) {
            doDeRebusAntiquis = TRUE;
            
            IPSW7Inject = argv[i + 1];
            i++;
            continue;
        }
        
        if(strcmp(argv[i], "-a") == 0) {
            APFile = createAbstractFileFromFile(fopen(argv[i + 1], "rb"));
            if(!APFile) {
                XLOG(0, "cannot open %s\n", argv[i + 1]);
                exit(1);
            }
            i++;
            continue;
        }

		if(strcmp(argv[i], "-e") == 0) {
			numToRemove++;
			toRemove = realloc(toRemove, numToRemove * sizeof(int));
			toRemove[numToRemove - 1] = i + 1;
			i++;
			continue;
		}
		
		if(strcmp(argv[i], "-unlock") == 0) {
			unlockBaseband = TRUE;
			continue;
		}

		if(strcmp(argv[i], "-cleanup") == 0) {
			selfDestruct = TRUE;
			continue;
		}
		
		if(strcmp(argv[i], "-use39") == 0) {
			if(use46) {
				XLOG(0, "error: select only one of -use39 and -use46\n");
				exit(1);
			}
			use39 = TRUE;
			continue;
		}
		
		if(strcmp(argv[i], "-use46") == 0) {
			if(use39) {
				XLOG(0, "error: select only one of -use39 and -use46\n");
				exit(1);
			}
			use46 = TRUE;
			continue;
		}

		if(strcmp(argv[i], "-b") == 0) {
			applelogo = createAbstractFileFromFile(fopen(argv[i + 1], "rb"));
			if(!applelogo) {
				XLOG(0, "cannot open %s\n", argv[i + 1]);
				exit(1);
			}
			i++;
			continue;
		}

		if(strcmp(argv[i], "-r") == 0) {
			recoverymode = createAbstractFileFromFile(fopen(argv[i + 1], "rb"));
			if(!recoverymode) {
				XLOG(0, "cannot open %s\n", argv[i + 1]);
				exit(1);
			}
			i++;
			continue;
		}

		if(strcmp(argv[i], "-3") == 0) {
			bootloader39 = createAbstractFileFromFile(fopen(argv[i + 1], "rb"));
			if(!bootloader39) {
				XLOG(0, "cannot open %s\n", argv[i + 1]);
				exit(1);
			}
			i++;
			continue;
		}

		if(strcmp(argv[i], "-4") == 0) {
			bootloader46 = createAbstractFileFromFile(fopen(argv[i + 1], "rb"));
			if(!bootloader46) {
				XLOG(0, "cannot open %s\n", argv[i + 1]);
				exit(1);
			}
			i++;
			continue;
		}

		if(strcmp(argv[i], "-ota") == 0) {
			otaFile = createAbstractFileFromFile(fopen(argv[i + 1], "rb"));
			if(!otaFile) {
				XLOG(0, "cannot open %s\n", argv[i + 1]);
				exit(1);
			}
			i++;
			continue;
		}
	}

    char* bundleRoot;
    
    if(useJailbreak == TRUE){
        bundleRoot = "JailbreakBundles/";
    } else {
        bundleRoot = "FirmwareBundles/";
    }
    
    
	mergePaths = i;

	if(use39 || use46 || unlockBaseband || selfDestruct || bootloader39 || bootloader46) {
		if(!(bootloader39) || !(bootloader46)) {
			XLOG(0, "error: you must specify both bootloader files.\n");
			exit(1);
		} else {
			doBootNeuter = TRUE;
		}
	}

	info = parseIPSW2(argv[1], bundleRoot, &bundlePath, &outputState, useMemory);

	if(info == NULL) {
		XLOG(0, "error: Could not load IPSW\n");
		exit(1);
	}

    if(doDeRebusAntiquis) {
        info7 = parseIPSW2(IPSW7Inject, bundleRoot, &bundle7Path, &output7State, useMemory);
        if(info7 == NULL) {
            XLOG(0, "error: Could not load IPSW\n");
            exit(1);
        }
    }
    
	firmwarePatches = (Dictionary*)getValueByKey(info, "FilesystemPatches");

	int j;
    
	for(j = 0; j < numToRemove; j++) {
		removeKey(firmwarePatches, argv[toRemove[j]]);
	}
	free(toRemove);

	manifestFile = getFileFromOutputState(&outputState, "BuildManifest.plist");
	if (manifestFile) {
		size_t fileLength = manifestFile->getLength(manifestFile);
		char *plist = malloc(fileLength);
		manifestFile->read(manifestFile, plist, fileLength);
		manifestFile->close(manifestFile);
		manifest = createRoot(plist);
		free(plist);
	}

	if (otaFile) {
		if (mergeIdentities(manifest, otaFile) != 0) {
			XLOG(1, "cannot merge OTA BuildIdentity\n");
			exit(1);
		}
		otaFile->close(otaFile);
		manifestDirty = TRUE;
	}

    if(doDeRebusAntiquis) {
        /* inject de rebus antiquis */
        const char *AppleLogoPath;
        const char *APTicketPath;
        const char *OrigAppleLogoPath;
        const char *BatteryCharging0Path;
        const char *BatteryCharging1Path;
        const char *BatteryFullPath;
        const char *BatteryLow0Path;
        const char *BatteryLow1Path;
        const char *BatteryPluginPath;
        const char *RecoveryModePath;
        const char *OrigRecoveryModePath;
        const char *LLBPath;
        const char *iBootPath;
        const char *NewiBootPath;
        
        StringValue* NewiBootkeyValue;
        StringValue* NewiBootivValue;
        StringValue* OrigiBootVal;
        const char *NewiBootKEY;
        const char *NewiBootIV;
        unsigned int* ibotKey = NULL;
        unsigned int* ibotIV = NULL;
        
        const char *manifestPath;
        const char *manifestFileval;
        
        const char *baseAppleLogoPath;
        const char *baseBatteryCharging0Path;
        const char *baseBatteryCharging1Path;
        const char *baseBatteryFullPath;
        const char *baseBatteryLow0Path;
        const char *baseBatteryLow1Path;
        const char *baseBatteryPluginPath;
        const char *baseRecoveryModePath;
        const char *baseLLBPath;
        const char *baseiBootPath;
        
        size_t fileLength;
        
        // check ios ver
        IntegerValue* checkiosValue = (IntegerValue*) getValueByKey(info, "isIOS");
        if(checkiosValue){
            isIOS = checkiosValue->value; // 433 or 435 or 9
            
            /* Difference in behavior for each OS version *
             * 433  : Enable hook for old iBoot, and Enable patches for iOS 4 iBoot
             * 435  : Enable patches for iOS 4 iBoot
             * 9    : Enable a patch that prevents the boot-partition value of nvram from being reset to iBoot.
             */
            
        }

        baseFWPath = (Dictionary*)getValueByKey(info7, "FirmwarePath");
        baseFWDict = (Dictionary*) baseFWPath->values;
        
        firmwareInject = (Dictionary*)getValueByKey(info, "FirmwareInject");
        injectDict = (Dictionary*) firmwareInject->values;
        
        while(injectDict != NULL) {
            injectFileValue = (StringValue*) getValueByKey(injectDict, "File");
            injectmanifestValue = (StringValue*) getValueByKey(injectDict, "manifest");
            NewiBootkeyValue = (StringValue*) getValueByKey(injectDict, "Key");
            NewiBootivValue = (StringValue*) getValueByKey(injectDict, "IV");
            
            if(strcmp(injectDict->dValue.key, "APTicket") == 0) {
                APTicketPath = injectFileValue->value;
                
                char* plist;
                plist = (char*) malloc(APFile->getLength(APFile));
                APFile->read(APFile, plist, APFile->getLength(APFile));
                APFile->close(APFile);
                shsh = createRoot(plist);
                free(plist);
                
                unsigned char* ticket;
                size_t ticket_sz;
                DataValue* ticketValue = (DataValue*) getValueByKey(shsh, "APTicket");
                
                if(ticketValue){
                    /* apticket */
                    ticket_sz = ticketValue->len;
                    ticket = malloc(ticket_sz);
                    memcpy(ticket, ticketValue->value, ticket_sz);
                    ticket = ticketValue->value;
                    
                    size_t scab_sz = scab_template_img3_len;
                    void* scab = malloc(scab_sz);
                    memcpy(scab, scab_template_img3, scab_sz);
                    
                    AbstractFile* inTicket = createAbstractFileFromMemoryFile((void**)&ticket, &ticket_sz);
                    AbstractFile* template = createAbstractFileFromMemoryFile((void**)&scab, &scab_sz);
                    
                    AbstractFile* inAPTicket = openAbstractFile(inTicket);
                    char* ap;
                    size_t apsz;
                    AbstractFile* outAPTicket = createAbstractFileFromMemoryFile((void**)&ap, &apsz);
                    AbstractFile* newAPTicket = duplicateAbstractFile2(template, outAPTicket, NULL, NULL, NULL);
                    
                    size_t inDataSize = (size_t) inAPTicket->getLength(inAPTicket);
                    char* inData = (char*) malloc(inDataSize);
                    inAPTicket->read(inAPTicket, inData, inDataSize);
                    inAPTicket->close(inAPTicket);
                    
                    newAPTicket->write(newAPTicket, inData, inDataSize);
                    newAPTicket->close(newAPTicket);
                    
                    addToOutput(&outputState, APTicketPath, ap, apsz);
                    
                } else {
                    XLOG(1, "cannot read apticket\n");
                    exit(1);
                }
            }
            
            if(strcmp(injectDict->dValue.key, "AppleLogo") == 0) {
                AppleLogoPath = injectFileValue->value;
            }
            if(strcmp(injectDict->dValue.key, "OrigAppleLogo") == 0) {
                OrigAppleLogoPath = injectFileValue->value;
            }
            if(strcmp(injectDict->dValue.key, "BatteryCharging0") == 0) {
                BatteryCharging0Path = injectFileValue->value;
            }
            if(strcmp(injectDict->dValue.key, "BatteryCharging1") == 0) {
                BatteryCharging1Path = injectFileValue->value;
            }
            if(strcmp(injectDict->dValue.key, "BatteryFull") == 0) {
                BatteryFullPath = injectFileValue->value;
            }
            if(strcmp(injectDict->dValue.key, "BatteryLow0") == 0) {
                BatteryLow0Path = injectFileValue->value;
            }
            if(strcmp(injectDict->dValue.key, "BatteryLow1") == 0) {
                BatteryLow1Path = injectFileValue->value;
            }
            if(strcmp(injectDict->dValue.key, "BatteryPlugin") == 0) {
                BatteryPluginPath = injectFileValue->value;
            }
            if(strcmp(injectDict->dValue.key, "RecoveryMode") == 0) {
                RecoveryModePath = injectFileValue->value;
            }
            if(strcmp(injectDict->dValue.key, "OrigRecoveryMode") == 0) {
                OrigRecoveryModePath = injectFileValue->value;
            }
            if(strcmp(injectDict->dValue.key, "LLB") == 0) {
                LLBPath = injectFileValue->value;
            }
            if(strcmp(injectDict->dValue.key, "iBoot") == 0) {
                iBootPath = injectFileValue->value;
                OrigiBootVal = injectFileValue;
            }
            if(strcmp(injectDict->dValue.key, "NewiBoot") == 0) {
                NewiBootPath = injectFileValue->value;
                NewiBootKEY = NewiBootkeyValue->value;
                NewiBootIV = NewiBootivValue->value;
            }
            if(strcmp(injectDict->dValue.key, "manifest") == 0) {
                manifestPath = injectFileValue->value;
                manifestFileval = injectmanifestValue->value;
                
                char *manifestFilePath = malloc(sizeof(char) * (strlen(bundlePath) + strlen(manifestFileval) + 2));
                strcpy(manifestFilePath, bundlePath);
                strcat(manifestFilePath, "/");
                strcat(manifestFilePath, manifestFileval);
                
                AbstractFile *NewManifest = createAbstractFileFromFile(fopen(manifestFilePath, "rb"));
                fileLength = NewManifest->getLength(NewManifest);
                char *manifestnewfile = malloc(fileLength);
                NewManifest->read(NewManifest, manifestnewfile, fileLength);
                NewManifest->close(NewManifest);
                addToOutput(&outputState, manifestPath, manifestnewfile, fileLength);
                free(manifestFilePath);
            }
            injectDict = (Dictionary*) injectDict->dValue.next;
        }
        
        /* 2nd, get val base os image name */
        
        while(baseFWDict != NULL) {
            baseFWFileValue = (StringValue*) getValueByKey(baseFWDict, "File");
            if(strcmp(baseFWDict->dValue.key, "AppleLogo") == 0) {
                baseAppleLogoPath = baseFWFileValue->value;
                
                AbstractFile *FileAppleLogo = getFileFromOutputState(&output7State, baseAppleLogoPath);
                fileLength = FileAppleLogo->getLength(FileAppleLogo);
                char *applelogo = malloc(fileLength);
                FileAppleLogo->read(FileAppleLogo, applelogo, fileLength);
                FileAppleLogo->close(FileAppleLogo);
                addToOutput(&outputState, AppleLogoPath, applelogo, fileLength);
                
                AbstractFile *FileorigAppleLogo = getFileFromOutputState(&outputState, OrigAppleLogoPath);
                fileLength = FileorigAppleLogo->getLength(FileorigAppleLogo);
                char *origapplelogo = malloc(fileLength);
                FileorigAppleLogo->read(FileorigAppleLogo, origapplelogo, fileLength);
                
                // 'logo' -> 'logb'
                *(uint8_t*)(origapplelogo+0x10) = 0x62;
                *(uint8_t*)(origapplelogo+0x20) = 0x62;
                FileorigAppleLogo->close(FileorigAppleLogo);
                addToOutput(&outputState, OrigAppleLogoPath, origapplelogo, fileLength);
            }
            if(strcmp(baseFWDict->dValue.key, "BatteryCharging0") == 0) {
                baseBatteryCharging0Path = baseFWFileValue->value;
                
                AbstractFile *FileBatteryCharging0 = getFileFromOutputState(&output7State, baseBatteryCharging0Path);
                fileLength = FileBatteryCharging0->getLength(FileBatteryCharging0);
                char *batterycharging0 = malloc(fileLength);
                FileBatteryCharging0->read(FileBatteryCharging0, batterycharging0, fileLength);
                FileBatteryCharging0->close(FileBatteryCharging0);
                addToOutput(&outputState, BatteryCharging0Path, batterycharging0, fileLength);
            }
            if(strcmp(baseFWDict->dValue.key, "BatteryCharging1") == 0) {
                baseBatteryCharging1Path = baseFWFileValue->value;
                
                AbstractFile *FileBatteryCharging1 = getFileFromOutputState(&output7State, baseBatteryCharging1Path);
                fileLength = FileBatteryCharging1->getLength(FileBatteryCharging1);
                char *batterycharging1 = malloc(fileLength);
                FileBatteryCharging1->read(FileBatteryCharging1, batterycharging1, fileLength);
                FileBatteryCharging1->close(FileBatteryCharging1);
                addToOutput(&outputState, BatteryCharging1Path, batterycharging1, fileLength);
            }
            if(strcmp(baseFWDict->dValue.key, "BatteryFull") == 0) {
                baseBatteryFullPath = baseFWFileValue->value;
                
                AbstractFile *FileBatteryFull = getFileFromOutputState(&output7State, baseBatteryFullPath);
                fileLength = FileBatteryFull->getLength(FileBatteryFull);
                char *batteryfull = malloc(fileLength);
                FileBatteryFull->read(FileBatteryFull, batteryfull, fileLength);
                FileBatteryFull->close(FileBatteryFull);
                addToOutput(&outputState, BatteryFullPath, batteryfull, fileLength);
            }
            if(strcmp(baseFWDict->dValue.key, "BatteryLow0") == 0) {
                baseBatteryLow0Path = baseFWFileValue->value;
                
                AbstractFile *FileBatteryLow0 = getFileFromOutputState(&output7State, baseBatteryLow0Path);
                fileLength = FileBatteryLow0->getLength(FileBatteryLow0);
                char *batterylow0 = malloc(fileLength);
                FileBatteryLow0->read(FileBatteryLow0, batterylow0, fileLength);
                FileBatteryLow0->close(FileBatteryLow0);
                addToOutput(&outputState, BatteryLow0Path, batterylow0, fileLength);
            }
            if(strcmp(baseFWDict->dValue.key, "BatteryLow1") == 0) {
                baseBatteryLow1Path = baseFWFileValue->value;
                
                AbstractFile *FileBatteryLow1 = getFileFromOutputState(&output7State, baseBatteryLow1Path);
                fileLength = FileBatteryLow1->getLength(FileBatteryLow1);
                char *batterylow1 = malloc(fileLength);
                FileBatteryLow1->read(FileBatteryLow1, batterylow1, fileLength);
                FileBatteryLow1->close(FileBatteryLow1);
                addToOutput(&outputState, BatteryLow1Path, batterylow1, fileLength);
            }
            if(strcmp(baseFWDict->dValue.key, "BatteryPlugin") == 0) {
                baseBatteryPluginPath = baseFWFileValue->value;
                
                AbstractFile *FileBatteryPlugin = getFileFromOutputState(&output7State, baseBatteryPluginPath);
                fileLength = FileBatteryPlugin->getLength(FileBatteryPlugin);
                char *batteryplugin = malloc(fileLength);
                FileBatteryPlugin->read(FileBatteryPlugin, batteryplugin, fileLength);
                FileBatteryPlugin->close(FileBatteryPlugin);
                addToOutput(&outputState, BatteryPluginPath, batteryplugin, fileLength);
            }
            if(strcmp(baseFWDict->dValue.key, "RecoveryMode") == 0) {
                baseRecoveryModePath = baseFWFileValue->value;
                
                AbstractFile *FileRecoveryMode = getFileFromOutputState(&output7State, baseRecoveryModePath);
                fileLength = FileRecoveryMode->getLength(FileRecoveryMode);
                char *recoverymode = malloc(fileLength);
                FileRecoveryMode->read(FileRecoveryMode, recoverymode, fileLength);
                FileRecoveryMode->close(FileRecoveryMode);
                addToOutput(&outputState, RecoveryModePath, recoverymode, fileLength);
                
                AbstractFile *FileorigRecoveryMode = getFileFromOutputState(&outputState, OrigRecoveryModePath);
                fileLength = FileorigRecoveryMode->getLength(FileorigRecoveryMode);
                char *origrecoverymode = malloc(fileLength);
                FileorigRecoveryMode->read(FileorigRecoveryMode, origrecoverymode, fileLength);
                
                // 'recm' -> 'recb'
                *(uint8_t*)(origrecoverymode+0x10) = 0x62;
                *(uint8_t*)(origrecoverymode+0x20) = 0x62;
                FileorigRecoveryMode->close(FileorigRecoveryMode);
                addToOutput(&outputState, OrigRecoveryModePath, origrecoverymode, fileLength);
            }
            if(strcmp(baseFWDict->dValue.key, "LLB") == 0) {
                baseLLBPath = baseFWFileValue->value;
                
                AbstractFile *FileLLB = getFileFromOutputState(&output7State, baseLLBPath);
                fileLength = FileLLB->getLength(FileLLB);
                char *illb = malloc(fileLength);
                FileLLB->read(FileLLB, illb, fileLength);
                FileLLB->close(FileLLB);
                addToOutput(&outputState, LLBPath, illb, fileLength);
            }
            if(strcmp(baseFWDict->dValue.key, "iBoot") == 0) {
                // newiBoot
                AbstractFile *NewiBootFile = getFileFromOutputState(&outputState, iBootPath);
                fileLength = NewiBootFile->getLength(NewiBootFile);
                char *origiBootTmp = malloc(fileLength);
                size_t fileTmpLength = NewiBootFile->getLength(NewiBootFile);
                NewiBootFile->read(NewiBootFile, origiBootTmp, fileTmpLength);
                
                // xpwntool
                ibotKey = NULL;
                ibotIV = NULL;
                if(NewiBootKEY) {
                    sscanf(NewiBootKEY, "%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x",
                           &key[0], &key[1], &key[2], &key[3], &key[4], &key[5], &key[6], &key[7], &key[8],
                           &key[9], &key[10], &key[11], &key[12], &key[13], &key[14], &key[15],
                           &key[16], &key[17], &key[18], &key[19], &key[20], &key[21], &key[22], &key[23], &key[24],
                           &key[25], &key[26], &key[27], &key[28], &key[29], &key[30], &key[31]);
                    ibotKey = key;
                }
                if(NewiBootIV) {
                    sscanf(NewiBootIV, "%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x",
                           &iv[0], &iv[1], &iv[2], &iv[3], &iv[4], &iv[5], &iv[6], &iv[7], &iv[8],
                           &iv[9], &iv[10], &iv[11], &iv[12], &iv[13], &iv[14], &iv[15]);
                    ibotIV = iv;
                }
                
                AbstractFile* inFile = openAbstractFile2(NewiBootFile, ibotKey, ibotIV);
                size_t inFileLength = inFile->getLength(inFile);
                char *deciBoot = malloc(inFileLength);
                inFile->read(inFile, deciBoot, inFileLength);
                
                char* BootArgs = NULL;
                StringValue* BootArgsValue = (StringValue*) getValueByKey(info, "boot-args");
                if(BootArgsValue) {
                    BootArgs = BootArgsValue->value;
                }
                
                bool debug_enabled = ((BoolValue*) getValueByKey(info, "debug_enabled"))->value;
                
                // iBoot32Patcher
                iBoot32Patcher(deciBoot,                    // data
                               inFileLength,                // sz
                               true,                        // rsa
                               debug_enabled,               // debug_enabled
                               true, isIOS == 9 ? 1:0,      // ignore boot-partition, isiOS9?
                               true,                        // ignore boot-ramdisk
                               true,                        // setenv
                               isIOS == 433 ? true:false,   // old iBoot hooker
                               isIOS == 0 ? true:false,     // logo type patch
                               isIOS > 400 ? true:false,    // iOS 4 logo type patch
                               BootArgs                     // bootArgs
                               );
                
                AbstractFile* inRaw = createAbstractFileFromMemoryFile((void**)&deciBoot, &inFileLength);
                AbstractFile* template = createAbstractFileFromMemoryFile((void**)&origiBootTmp, &fileTmpLength);
                
                AbstractFile* inxFile = openAbstractFile(inRaw);
                char* pwniBoot;
                size_t pwniBootsz;
                AbstractFile* outxFile = createAbstractFileFromMemoryFile((void**)&pwniBoot, &pwniBootsz);
                AbstractFile* newFile = duplicateAbstractFile2(template, outxFile, ibotKey, ibotIV, NULL);
                if(newFile->type == AbstractFileTypeImg3) {
                    AbstractFile2* abstractFile2 = (AbstractFile2*) newFile;
                    abstractFile2->setKey(abstractFile2, ibotKey, ibotIV);
                }
                
                size_t inDataSize = (size_t) inxFile->getLength(inxFile);
                char* inData = (char*) malloc(inDataSize);
                inxFile->read(inxFile, inData, inDataSize);
                inxFile->close(inxFile);
                
                newFile->write(newFile, inData, inDataSize);
                newFile->close(newFile);
                
                // 'ibot' -> 'ibob'
                *(uint8_t*)(pwniBoot+0x10) = 0x62;
                *(uint8_t*)(pwniBoot+0x20) = 0x62;
                addToOutput(&outputState, NewiBootPath, pwniBoot, fileTmpLength /* pwniBootsz? */);

                // iBoot7
                baseiBootPath = baseFWFileValue->value;
                AbstractFile *FileiBoot = getFileFromOutputState(&output7State, baseiBootPath);
                fileLength = FileiBoot->getLength(FileiBoot);
                char *ibot = malloc(fileLength);
                FileiBoot->read(FileiBoot, ibot, fileLength);
                FileiBoot->close(FileiBoot);
                addToOutput(&outputState, iBootPath, ibot, fileLength);
                
            }
            baseFWDict = (Dictionary*) baseFWDict->dValue.next;
        }
        
        int tarType = ((IntegerValue*) getValueByKey(info, "TarType"))->value;
        // 6: bin6_tar/bin6_tar_len
        // 4: bin4_tar/bin4_tar_len
        
        /* exploit */
        int exploitType = ((IntegerValue*) getValueByKey(info7, "exploitType"))->value; // 1: n90-ramdiskI
        
        /* exploit list
         * 0: n42-11b554a
         * 1: n90-11d257
         * 2: n94-11d257
         * 3: j1-11d257
         * 4: j2a-11d257
         * 5: n42-11d257
         * 6: n78-11d257
         * 7: n48-11d201
         * 8: n48-11b554a
         * 9: k93a-11d257
         */
        
        if(exploitType == 1){
            void *expbuf = malloc(ramdiskI_n90_dmg_len);
            memcpy(expbuf, ramdiskI_n90_dmg, ramdiskI_n90_dmg_len);
            exploitDisk = createAbstractFileFromMemory((void**)&expbuf, ramdiskI_n90_dmg_len);
            exploitDiskSize = ramdiskI_n90_dmg_len;
        }
        
        /* ramdisk utils */
        if(tarType == 4){
            void *tarbuf = malloc(bin4_tar_len);
            memcpy(tarbuf, bin4_tar, bin4_tar_len);
            tarInjectFile = createAbstractFileFromMemory((void**)&tarbuf, bin4_tar_len);
            tarInjectSize = bin4_tar_len;
        }
        
        if(tarType == 6){
            void *tarbuf = malloc(bin6_tar_len);
            memcpy(tarbuf, bin6_tar, bin6_tar_len);
            tarInjectFile = createAbstractFileFromMemory((void**)&tarbuf, bin6_tar_len);
            tarInjectSize = bin6_tar_len;
        }
        
        if(isIOS < 400){
            if(exploitType == 1){
                void *parbuf = malloc(partition_n90_sh_len);
                memcpy(parbuf, partition_n90_sh, partition_n90_sh_len);
                partitionFile = createAbstractFileFromMemory((void**)&parbuf, partition_n90_sh_len);
                partitionSize = partition_n90_sh_len;
            }
        }
    }
    
    firmwarePatches = (Dictionary*)getValueByKey(info, "FirmwarePatches");
    patchDict = (Dictionary*) firmwarePatches->values;
    while(patchDict != NULL) {
        fileValue = (StringValue*) getValueByKey(patchDict, "File");
        
        StringValue* keyValue = (StringValue*) getValueByKey(patchDict, "Key");
        StringValue* ivValue = (StringValue*) getValueByKey(patchDict, "IV");
        pKey = NULL;
        pIV = NULL;
        
        if(keyValue) {
            sscanf(keyValue->value, "%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x",
                   &key[0], &key[1], &key[2], &key[3], &key[4], &key[5], &key[6], &key[7], &key[8],
                   &key[9], &key[10], &key[11], &key[12], &key[13], &key[14], &key[15],
                   &key[16], &key[17], &key[18], &key[19], &key[20], &key[21], &key[22], &key[23], &key[24],
                   &key[25], &key[26], &key[27], &key[28], &key[29], &key[30], &key[31]);
            
            pKey = key;
        }
        
        if(ivValue) {
            sscanf(ivValue->value, "%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x",
                   &iv[0], &iv[1], &iv[2], &iv[3], &iv[4], &iv[5], &iv[6], &iv[7], &iv[8],
                   &iv[9], &iv[10], &iv[11], &iv[12], &iv[13], &iv[14], &iv[15]);
            pIV = iv;
        }
        
        BoolValue *isPlainValue = (BoolValue *)getValueByKey(patchDict, "IsPlain");
        int isPlain = (isPlainValue && isPlainValue->value);
        
        if(strcmp(patchDict->dValue.key, "Restore Ramdisk") == 0) {
            ramdiskFSPathInIPSW = fileValue->value;
            if(pKey) {
                memcpy(ramdiskKey, key, sizeof(key));
                memcpy(ramdiskIV, iv, sizeof(iv));
                pRamdiskKey = ramdiskKey;
                pRamdiskIV = ramdiskIV;
            } else {
                pRamdiskKey = NULL;
                pRamdiskIV = NULL;
            }
        }
        
        if(strcmp(patchDict->dValue.key, "Update Ramdisk") == 0) {
            updateRamdiskFSPathInIPSW = fileValue->value;
        }
        
        patchValue = (StringValue*) getValueByKey(patchDict, "Patch2");
        if(patchValue) {
            if(noWipe) {
                XLOG(0, "%s: ", patchDict->dValue.key); fflush(stdout);
                doPatch(patchValue, fileValue, bundlePath, &outputState, pKey, pIV, useMemory, isPlain);
                patchDict = (Dictionary*) patchDict->dValue.next;
                continue; /* skip over the normal Patch */
            }
        }
        
        patchValue = (StringValue*) getValueByKey(patchDict, "Patch");
        if(patchValue) {
            XLOG(0, "%s: ", patchDict->dValue.key); fflush(stdout);
            doPatch(patchValue, fileValue, bundlePath, &outputState, pKey, pIV, useMemory, isPlain);
        }
        
        if(strcmp(patchDict->dValue.key, "AppleLogo") == 0 && applelogo) {
            XLOG(0, "replacing %s\n", fileValue->value); fflush(stdout);
            ASSERT((imageBuffer = replaceBootImage(getFileFromOutputState(&outputState, fileValue->value), pKey, pIV, applelogo, &imageSize)) != NULL, "failed to use new image");
            addToOutput(&outputState, fileValue->value, imageBuffer, imageSize);
        }
        
        if(strcmp(patchDict->dValue.key, "RecoveryMode") == 0 && recoverymode) {
            XLOG(0, "replacing %s\n", fileValue->value); fflush(stdout);
            ASSERT((imageBuffer = replaceBootImage(getFileFromOutputState(&outputState, fileValue->value), pKey, pIV, recoverymode, &imageSize)) != NULL, "failed to use new image");
            addToOutput(&outputState, fileValue->value, imageBuffer, imageSize);
        }
        
        BoolValue *decryptValue = (BoolValue *)getValueByKey(patchDict, "Decrypt");
        StringValue *decryptPathValue = (StringValue*) getValueByKey(patchDict, "DecryptPath");
        if ((decryptValue && decryptValue->value) || decryptPathValue) {
            XLOG(0, "%s: ", patchDict->dValue.key); fflush(stdout);
            doDecrypt(decryptPathValue, fileValue, bundlePath, &outputState, pKey, pIV, useMemory);
            if(strcmp(patchDict->dValue.key, "Restore Ramdisk") == 0) {
                pRamdiskKey = NULL;
                pRamdiskIV = NULL;
            }
            if (decryptPathValue  && manifest) {
                ArrayValue *buildIdentities = (ArrayValue *)getValueByKey(manifest, "BuildIdentities");
                if (buildIdentities) {
                    for (i = 0; i < buildIdentities->size; i++) {
                        StringValue *path;
                        Dictionary *dict = (Dictionary *)buildIdentities->values[i];
                        if (!dict) continue;
                        dict = (Dictionary *)getValueByKey(dict, "Manifest");
                        if (!dict) continue;
                        dict = (Dictionary *)getValueByKey(dict, patchDict->dValue.key);
                        if (!dict) continue;
                        dict = (Dictionary *)getValueByKey(dict, "Info");
                        if (!dict) continue;
                        path = (StringValue *)getValueByKey(dict, "Path");
                        if (!path) continue;
                        free(path->value);
                        path->value = strdup(decryptPathValue->value);
                        manifestDirty = TRUE;
                    }
                }
            }
        }
        
        patchDict = (Dictionary*) patchDict->dValue.next;
    }
    
    if (manifestDirty && manifest) {
        manifestFile = getFileFromOutputStateForReplace(&outputState, "BuildManifest.plist");
        if (manifestFile) {
            char *plist = getXmlFromRoot(manifest);
            manifestFile->write(manifestFile, plist, strlen(plist));
            manifestFile->close(manifestFile);
            free(plist);
        }
        releaseDictionary(manifest);
    }
    
    fileValue = (StringValue*) getValueByKey(info, "RootFilesystem");
    rootFSPathInIPSW = fileValue->value;
    
    size_t defaultRootSize = ((IntegerValue*) getValueByKey(info, "RootFilesystemSize"))->value;
    for(j = mergePaths; j < argc; j++) {
        AbstractFile* tarFile = createAbstractFileFromFile(fopen(argv[j], "rb"));
        if(tarFile) {
            defaultRootSize += (tarFile->getLength(tarFile) + 1024 * 1024 - 1) / (1024 * 1024); // poor estimate
            tarFile->close(tarFile);
        }
    }
    
    // jailbreak utils
    int IsCydia;
    int IsUntether;
    const char *UntetherInjectPath;
    const char *CydiaInjectPath;
    AbstractFile* CydiaFile;
    AbstractFile* UntetherFile;
    
    StringValue* CydiaInjectValue = (StringValue*) getValueByKey(info, "PackagePath");
    if(CydiaInjectValue) {
        CydiaInjectPath = CydiaInjectValue->value;
        CydiaFile = createAbstractFileFromFile(fopen(CydiaInjectPath, "rb"));
        if(CydiaFile) {
            IsCydia = 1;
            defaultRootSize += (CydiaFile->getLength(CydiaFile) + 1024 * 1024 - 1) / (1024 * 1024);
            CydiaFile->close(CydiaFile);
        }
    }
    
    StringValue* UntetherInjectValue = (StringValue*) getValueByKey(info, "UntetherPath");
    if(UntetherInjectValue) {
        UntetherInjectPath = UntetherInjectValue->value;
        
        UntetherFile = createAbstractFileFromFile(fopen(UntetherInjectPath, "rb"));
        if(UntetherFile) {
            IsUntether = 1;
            defaultRootSize += (UntetherFile->getLength(UntetherFile) + 1024 * 1024 - 1) / (1024 * 1024);
            UntetherFile->close(UntetherFile);
        }
    }
    
	minimumRootSize = defaultRootSize * 1024 * 1024;
	minimumRootSize -= minimumRootSize % 512;

	if(preferredRootSize == 0) {	
		preferredRootSize = defaultRootSize + preferredRootSizeAdd;
	}

	rootSize =  preferredRootSize * 1024 * 1024;
	rootSize -= rootSize % 512;

	if(useMemory) {
		buffer = calloc(1, rootSize);
	} else {
		buffer = NULL;
	}

	if(buffer == NULL) {
		XLOG(2, "using filesystem backed temporary storage\n");
	}

	extractDmg(
		createAbstractFileFromFileVault(getFileFromOutputState(&outputState, rootFSPathInIPSW), ((StringValue*)getValueByKey(info, "RootFilesystemKey"))->value),
		openRoot((void**)&buffer, &rootSize), -1);

	
	rootFS = IOFuncFromAbstractFile(openRoot((void**)&buffer, &rootSize));
	rootVolume = openVolume(rootFS);
	XLOG(0, "Growing root to minimum: %ld\n", (long) defaultRootSize); fflush(stdout);
	grow_hfs(rootVolume, minimumRootSize);
	if(rootSize > minimumRootSize) {
		XLOG(0, "Growing root: %ld\n", (long) preferredRootSize); fflush(stdout);
		grow_hfs(rootVolume, rootSize);
	}
	
	firmwarePatches = (Dictionary*)getValueByKey(info, "FilesystemPatches");
	patchArray = (ArrayValue*) firmwarePatches->values;
	while(patchArray != NULL) {
		for(i = 0; i < patchArray->size; i++) {
			patchDict = (Dictionary*) patchArray->values[i];
			fileValue = (StringValue*) getValueByKey(patchDict, "File");
					
			actionValue = (StringValue*) getValueByKey(patchDict, "Action"); 
			if(strcmp(actionValue->value, "ReplaceKernel") == 0) {
				pathValue = (StringValue*) getValueByKey(patchDict, "Path");
				XLOG(0, "replacing kernel... %s -> %s\n", fileValue->value, pathValue->value); fflush(stdout);
				add_hfs(rootVolume, getFileFromOutputState(&outputState, fileValue->value), pathValue->value);
			} if(strcmp(actionValue->value, "Patch") == 0) {
				patchValue = (StringValue*) getValueByKey(patchDict, "Patch");
				patchPath = (char*) malloc(sizeof(char) * (strlen(bundlePath) + strlen(patchValue->value) + 2));
				strcpy(patchPath, bundlePath);
				strcat(patchPath, "/");
				strcat(patchPath, patchValue->value);
				
				XLOG(0, "patching %s (%s)... ", fileValue->value, patchPath);
				doPatchInPlace(rootVolume, fileValue->value, patchPath);
				free(patchPath);
			}
		}
		
		patchArray = (ArrayValue*) patchArray->dValue.next;
	}
	
    for(; mergePaths < argc; mergePaths++) {
        XLOG(0, "merging %s\n", argv[mergePaths]);
        AbstractFile* tarFile = createAbstractFileFromFile(fopen(argv[mergePaths], "rb"));
        if(tarFile == NULL) {
            XLOG(1, "cannot find %s, make sure your slashes are in the right direction\n", argv[mergePaths]);
            releaseOutput(&outputState);
            closeRoot(buffer);
            exit(0);
        }
        if (tarFile->getLength(tarFile)) hfs_untar(rootVolume, tarFile);
        tarFile->close(tarFile);
    }
    
    if(IsCydia){
		XLOG(0, "merging %s\n", CydiaInjectPath);
		AbstractFile* CydiatarFile = createAbstractFileFromFile(fopen(CydiaInjectPath, "rb"));
		if(CydiatarFile == NULL) {
			XLOG(1, "cannot find %s, make sure your slashes are in the right direction\n", CydiaInjectPath);
			releaseOutput(&outputState);
			closeRoot(buffer);
			exit(0);
		}
		if (CydiatarFile->getLength(CydiatarFile)) hfs_untar(rootVolume, CydiatarFile);
		CydiatarFile->close(CydiatarFile);
    }
    
    if(IsUntether){
        XLOG(0, "merging %s\n", UntetherInjectPath);
        AbstractFile* UntethertarFile = createAbstractFileFromFile(fopen(UntetherInjectPath, "rb"));
        if(UntethertarFile == NULL) {
            XLOG(1, "cannot find %s, make sure your slashes are in the right direction\n", UntetherInjectPath);
            releaseOutput(&outputState);
            closeRoot(buffer);
            exit(0);
        }
        if (UntethertarFile->getLength(UntethertarFile)) hfs_untar(rootVolume, UntethertarFile);
        UntethertarFile->close(UntethertarFile);
    }
	
	if(pRamdiskKey) {
		ramdiskFS = IOFuncFromAbstractFile(openAbstractFile2(getFileFromOutputStateForOverwrite(&outputState, ramdiskFSPathInIPSW), pRamdiskKey, pRamdiskIV));
	} else {
		XLOG(0, "unencrypted ramdisk\n");
		ramdiskFS = IOFuncFromAbstractFile(openAbstractFile(getFileFromOutputStateForOverwrite(&outputState, ramdiskFSPathInIPSW)));
	}
	ramdiskVolume = openVolume(ramdiskFS);
    
    if(doDeRebusAntiquis) {
        size_t psize = partitionSize;
        size_t tsize = tarInjectSize;
        size_t pdsize = exploitDiskSize;
        size_t allsize = psize + tsize + pdsize;
        ramdiskGrow = ramdiskGrow + allsize/(ramdiskVolume->volumeHeader->blockSize) + 64;
    }
    
	XLOG(0, "growing ramdisk: %d -> %d\n", ramdiskVolume->volumeHeader->totalBlocks * ramdiskVolume->volumeHeader->blockSize, (ramdiskVolume->volumeHeader->totalBlocks + ramdiskGrow) * ramdiskVolume->volumeHeader->blockSize);
	grow_hfs(ramdiskVolume, (ramdiskVolume->volumeHeader->totalBlocks + ramdiskGrow) * ramdiskVolume->volumeHeader->blockSize);

	firmwarePatches = (Dictionary*)getValueByKey(info, "RamdiskPatches");
	if(firmwarePatches != NULL) {
		patchDict = (Dictionary*) firmwarePatches->values;
		while(patchDict != NULL) {
			fileValue = (StringValue*) getValueByKey(patchDict, "File");

			patchValue = (StringValue*) getValueByKey(patchDict, "Patch");
			if(patchValue) {
				patchPath = (char*) malloc(sizeof(char) * (strlen(bundlePath) + strlen(patchValue->value) + 2));
				strcpy(patchPath, bundlePath);
				strcat(patchPath, "/");
				strcat(patchPath, patchValue->value);

				XLOG(0, "patching %s (%s)... ", fileValue->value, patchPath);
				doPatchInPlace(ramdiskVolume, fileValue->value, patchPath);
				free(patchPath);
			}
		
			patchDict = (Dictionary*) patchDict->dValue.next;
		}
	}

	if(doBootNeuter) {
		firmwarePatches = (Dictionary*)getValueByKey(info, "BasebandPatches");
		if(firmwarePatches != NULL) {
			patchDict = (Dictionary*) firmwarePatches->values;
			while(patchDict != NULL) {
				pathValue = (StringValue*) getValueByKey(patchDict, "Path");

				fileValue = (StringValue*) getValueByKey(patchDict, "File");		
				if(fileValue) {
					XLOG(0, "copying %s -> %s... ", fileValue->value, pathValue->value); fflush(stdout);
					if(copyAcrossVolumes(ramdiskVolume, rootVolume, fileValue->value, pathValue->value)) {
						patchValue = (StringValue*) getValueByKey(patchDict, "Patch");
						if(patchValue) {
							patchPath = malloc(sizeof(char) * (strlen(bundlePath) + strlen(patchValue->value) + 2));
							strcpy(patchPath, bundlePath);
							strcat(patchPath, "/");
							strcat(patchPath, patchValue->value);
							XLOG(0, "patching %s (%s)... ", pathValue->value, patchPath); fflush(stdout);
							doPatchInPlace(rootVolume, pathValue->value, patchPath);
							free(patchPath);
						}
					}
				}

				if(strcmp(patchDict->dValue.key, "Bootloader 3.9") == 0 && bootloader39 != NULL) {
					add_hfs(rootVolume, bootloader39, pathValue->value);
				}

				if(strcmp(patchDict->dValue.key, "Bootloader 4.6") == 0 && bootloader46 != NULL) {
					add_hfs(rootVolume, bootloader46, pathValue->value);
				}
				
				patchDict = (Dictionary*) patchDict->dValue.next;
			}
		}
	
		fixupBootNeuterArgs(rootVolume, unlockBaseband, selfDestruct, use39, use46);
	}

	StringValue* optionsValue = (StringValue*) getValueByKey(info, "RamdiskOptionsPath");
	const char *optionsPlist = optionsValue ? optionsValue->value : "/usr/local/share/restore/options.plist";
	createRestoreOptions(ramdiskVolume, optionsPlist, preferredRootSize, updateBB, flashNOR);
    
    if(doDeRebusAntiquis) {
        
        const char *ReBootPath = "/sbin/reboot";
        const char *ReBootRePath = "/sbin/reboot_";
        move(ReBootPath, ReBootRePath, ramdiskVolume);

        XLOG(0, "injecting partition ...\n");
        if (isIOS < 400) add_hfs(ramdiskVolume, partitionFile, ReBootPath);
        
        XLOG(0, "injecting exploit ...\n");
        add_hfs(ramdiskVolume, exploitDisk, "ramdiskI.dmg");
        
        if (isIOS > 400) {
            /* Mitigation for old patchers */
            size_t dummy_sz = 1;
            void *dummy = malloc(1);
            memset(dummy, 'A', 0x1);
            AbstractFile* dummyFile = createAbstractFileFromMemoryFile((void**)&dummy, &dummy_sz);
            add_hfs(ramdiskVolume, dummyFile, "iBoot");
        }
        
        XLOG(0, "injecting tar ...\n");
        hfs_untar(ramdiskVolume, tarInjectFile);
        
        chmodFile(ReBootPath, 0755, ramdiskVolume);
        chownFile(ReBootPath, 0, 0, ramdiskVolume);
        
    }
    
	closeVolume(ramdiskVolume);
	CLOSE(ramdiskFS);

	if(updateRamdiskFSPathInIPSW)
		removeFileFromOutputState(&outputState, updateRamdiskFSPathInIPSW, TRUE);

	StringValue *removeBB = (StringValue*) getValueByKey(info, "DeleteBaseband");
	if (removeBB && removeBB->value[0])
		removeFileFromOutputState(&outputState, removeBB->value, FALSE);

	closeVolume(rootVolume);
	CLOSE(rootFS);

	buildDmg(openRoot((void**)&buffer, &rootSize), getFileFromOutputStateForReplace(&outputState, rootFSPathInIPSW), 2048);

	closeRoot(buffer);

	writeOutput(&outputState, outputIPSW);
	
	releaseDictionary(info);

	free(bundlePath);
	
	return 0;
}
