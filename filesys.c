// filesys.c - Copyright (c) 2025 Andre M. Maree / KSS Technologies (Pty) Ltd.

/*
    https://github.com/espressif/esp-usb-bridge/issues/17
    https://github.com/aliengreen/esp32_uart_bridge
*/
#include "hal_platform.h"
#include "errors_events.h"
#include "filesys.h"
#include "syslog.h"

#include <unistd.h>
#include <sys/stat.h>

#include "esp_littlefs.h"

#include "esp_flash.h"

// ###################################### General macros ###########################################

#define	debugFLAG					0xF000
#define	debugTIMING					(debugFLAG_GLOBAL & debugFLAG & 0x1000)
#define	debugTRACK					(debugFLAG_GLOBAL & debugFLAG & 0x2000)
#define	debugPARAM					(debugFLAG_GLOBAL & debugFLAG & 0x4000)
#define	debugRESULT					(debugFLAG_GLOBAL & debugFLAG & 0x8000)

// ##################################### Local structures ##########################################

// ###################################### Local constants ##########################################

#if ((appAEP == 1) && (appPLTFRM == HW_AC01) && (appOPTION < 2))

	static esp_vfs_littlefs_conf_t conf = {
		.base_path = "", .partition_label = NULL, .partition = NULL, //.sdcard = NULL,
		.format_if_mount_failed = true, .read_only = false, .dont_mount = false, .grow_on_mount = true,
	};

#else

	const esp_vfs_littlefs_conf_t conf = {
		.base_path = "", .partition_label = "littlefs", .partition = NULL,
		.format_if_mount_failed = true, .read_only = false, .dont_mount = false, .grow_on_mount = true,
	};

#endif

// ###################################### Local variables ##########################################

// ################################## Global/public variables ######################################

SemaphoreHandle_t shLFSmux = 0;

// ################################ Local ONLY utility functions ###################################

// ################################### Public functions ############################################

#if ((appAEP == 1) && (appPLTFRM == HW_AC01) && (appOPTION < 2))

int	xFileSysInit(void) {
	conf.partition_label = lfsPART_LABEL0;
	int iRV = esp_vfs_littlefs_register(&conf);
	if (iRV == ESP_OK) 
        return 1;				                        // found "FlashFS"
	conf.partition_label = lfsPART_LABEL1;
	iRV = esp_vfs_littlefs_register(&conf);
	if (iRV == ESP_OK)
        return 2;					                    // found "lfs
	conf.partition_label = lfsPART_LABEL2;
	iRV = esp_vfs_littlefs_register(&conf);
	return (iRV == ESP_OK) ? 3 : iRV;					// possibly found "littlefs"
}

#else

int	xFileSysInit(void) { return esp_vfs_littlefs_register(&conf); }

#endif

int xFileSysDeInit(void) { return esp_vfs_littlefs_unregister(conf.partition_label); }

int xFileSysFileWrite(const char * pFName, const char * pMode, char * pData) {
	int iRV = 0;
	FILE * fp = fopen(pFName, pMode);
	if (fp != NULL) {
		iRV = fputs(pData, fp);
		fclose(fp);
	}
	return (fp == NULL || iRV < erSUCCESS) ? -errno : iRV;
}

ssize_t xFileSysGetFileSize(const char * pccFname) {
	FILE *fp = fopen(pccFname, "r");
	if (fp == NULL)										// no such file or directory?
		return (errno == ENOENT) ? 0 : erFAILURE;		// return size of 0
	int iRV = fseek(fp, 0L, SEEK_END);					// seek to EOF
	if (iRV == erFAILURE)								// seek OK?
		return iRV;										// no, return error
	ssize_t Size = ftell(fp);
	fclose(fp);
	return Size;
}

int xFileSysListFileContent(report_t * psR, FILE * fp) {
	int iRV = 0;					// number of characters output
	ssize_t sRV = 0;				// number of bytes read
	u8_t * pu8Buf = malloc(filesysBUFSIZE);
	void * pAddr = 0;
	while (1) {
		sRV = fread(pu8Buf, sizeof(uint8_t), filesysBUFSIZE, fp);
		if (sRV < 1)
			break;
		iRV += report(psR, "%p" strNL "%'-+hhY", pAddr, sRV, pu8Buf);
		pAddr += sRV;
		vTaskDelay(pdMS_TO_TICKS(5));	// allow for kicking the WDT
	}
	iRV += report(psR, (psR->sFM.fsNL) ? strNL : strNUL);
	free(pu8Buf);
	return (sRV < 0) ? sRV : iRV;
}

int xFileSysFileDisplay(report_t * psR, const char * pccFN) {
	FILE * fp = fopen(pccFN, "r");
	int iRV = 0;
	if (fp != 0) 
		iRV = xFileSysListFileContent(psR, fp);
	fclose(fp);
	return iRV;
}

int xFileSysListFileInfo(report_t * psR, struct dirent * psDE, const char * pccDir) {
	struct stat sStat;
	int iRV = stat(psDE->d_name, &sStat);
	if (iRV < 0)
		return iRV;
	report(psR, "%s/%s  size=%d  tM=%R", pccDir, psDE->d_name, sStat.st_size, xTimeMakeTimeStamp(sStat.st_mtime, 0));
	if (psR->sFM.fsLev3)
		report(psR, "  tA=%R  tC=%R  Uid=%hu  Gid=%hu", xTimeMakeTimeStamp(sStat.st_atime, 0), xTimeMakeTimeStamp(sStat.st_ctime, 0), sStat.st_uid, sStat.st_gid);
	report(psR, strNL);
	if (psR->sFM.fsLev4)								// File content listing required?
		xFileSysFileDisplay(psR, psDE->d_name);
	return iRV;
}

int xFileSysListDirTree(report_t * psR, const char * pccDir) {
	DIR * psDir = opendir(pccDir);
	if (psDir == NULL)
        return -errno;
	while (1) {
		int iRV;
		struct dirent * psDE = readdir(psDir);
		if (psDE == NULL)								// end of [sub]directory?
			break;
		if (psDE->d_type == DT_REG) {					// File entry ?
			if (psR->sFM.fsLev2)						// List of files requested
				xFileSysListFileInfo(psR, psDE, pccDir);
		} else if (psDE->d_type == DT_DIR) {			// [sub]directory entry?
			size_t Size = strlen(pccDir) + strlen(psDE->d_name) + 3;
			void * pvBuf = calloc(1, Size);
			snprintfx(pvBuf, Size, "%s/%s" strNL, pccDir, psDE->d_name);
			iRV = xFileSysListDirTree(psR, pvBuf);		// RECURSE next level
			free(pvBuf);
			if (iRV < 0)
				SL_ERROR(iRV);
		} else {
			report(psR, "Error %s/%s t=%d" strNL, pccDir, psDE->d_name, psDE->d_type);
		}
	}
	return closedir(psDir);
}

int xFileSysListPartition(report_t * psR) {
	size_t total = 0, used = 0;
	int iRV = esp_littlefs_info(conf.partition_label, &total, &used);
	if (iRV != ESP_OK)
        return iRV;
	report(psR, "Partition size: %d, used: %d" strNL, total, used);
	if (psR->sFM.fsLev1)
        xFileSysListDirTree(psR, "/");
	return iRV;
}

int xFileSysMemoryToFile(const char * pcFName, u32_t xAddr, size_t xSize) {
	if (pcFName == NULL || *pcFName == 0 || xAddr == 0 || xSize == 0)
		return erINV_PARA;
	FILE * fp = fopen(pcFName, "w");
	if (fp == NULL)
        return -errno;
	void * pvBuf = malloc(filesysBUFSIZE);
	int iRV = erFAILURE;

	while(xSize) {
		size_t xNow = (xSize > filesysBUFSIZE) ? filesysBUFSIZE : xSize;
		iRV = esp_flash_read(esp_flash_default_chip, pvBuf, xAddr, xNow);
		if (iRV != ESP_OK)
            break;
		iRV = fwrite(pvBuf, 1, xNow, fp);
		if (iRV != xNow) {
			iRV = erFILE_WRITE;
			break;
		}
		xAddr += xNow;
		xSize -= xNow;
		iRV = erSUCCESS;
	}
	free(pvBuf);
	fclose(fp);
	return iRV;
}

int xFileSysFileToMemory(const char * pcFName, u32_t xAddr) {
	if (pcFName == NULL || *pcFName == 0 || xAddr == 0)
		return erINV_PARA;
	FILE * fp = fopen(pcFName, "r");
	if (fp == NULL)
        return -errno;
	// determine the file size
	int iRV = fseek(fp, 0L, SEEK_END);
	size_t xSize = ftell(fp);
	iRV = fseek(fp, 0L, SEEK_SET);
	// Allocate buffer
	void * pvBuf = malloc(filesysBUFSIZE);
	// now loop till whole file read and written to flash?
	while(xSize) {
		size_t xNow = (xSize > filesysBUFSIZE) ? filesysBUFSIZE : xSize;
		iRV = fread(pvBuf, 1, xNow, fp);
		if (iRV != xNow) {
			iRV = erFILE_READ;
			break;
		}
		iRV = esp_flash_write(esp_flash_default_chip, pvBuf, xAddr, xNow);
		if (iRV < ESP_OK)
            break;
		xAddr += xNow;
		xSize -= xNow;
		iRV = erSUCCESS;
	}
	free(pvBuf);
	fclose(fp);
	return iRV;
}

// ################################## File System test support #####################################

static void xFileSysTestFSdir(const char * pcDir) {
	#define	lfsFNAME_ORIG	"hello.txt"
	#define	lfsFNAME_RNAM	"foo.txt"
	report_t * psR = NULL;
	int iRV = mkdir(pcDir, 0);
	if (iRV == EEXIST)
		report(psR, "Directory '%s' already exist" strNL, pcDir);

	size_t Size = strlen(pcDir) + sizeof(lfsFNAME_ORIG) + 3; 
	char ccaBuf1[Size];
	snprintfx(ccaBuf1, Size, "%s/%s", pcDir, lfsFNAME_ORIG);
	FILE * f = fopen(ccaBuf1, "w");
	if (f == NULL) {
		report(psR, "Failed to open '%s' for writing" strNL, ccaBuf1);
		return;
	}
	fprintfx(f, "LittleFS fprintfx to %s!" strNL, ccaBuf1);
	fclose(f);

	Size = strlen(pcDir) + sizeof(lfsFNAME_RNAM) + 3; 
	char ccaBuf2[Size];
	snprintfx(ccaBuf2, Size, "%s/%s", pcDir, lfsFNAME_RNAM);
	struct stat st;
	if (stat(ccaBuf2, &st) == 0)
		unlink(ccaBuf2);
	if (rename(ccaBuf1, ccaBuf2) != 0) {
		report(psR, "Rename %s to %s failed" strNL, ccaBuf1, ccaBuf2);
		return;
	}
	f = fopen(ccaBuf2, "r");
	if (f == NULL) {
		report(psR, "Failed to open %s for reading" strNL, ccaBuf2);
		return;
	}
	char line[64];
	fgets(line, sizeof(line), f);
	fclose(f);
	char *pos = strchr(line, CHR_LF);
	if (pos)
		*pos = 0;
	report(psR, "Read from %s: '%s'" strNL, ccaBuf2, line);
}

static void xFileSysTestFSdelete(const char * pcDir) {
	char ccaBuf[strlen(pcDir) + 5];
	sprintf(ccaBuf, "%s/%s", pcDir, lfsFNAME_RNAM);
	int iRV = unlink(ccaBuf);
	if (iRV == 0)
        iRV = rmdir(pcDir);
	if (iRV != 0)
        SL_ERROR(errno);
}

void xFileSysTestFS(void) {
	report_t sRprt = { .sFM = { .fsLev1 = 1, .fsLev2 = 1, .fsLev3 = 1 } };
	ESP_ERROR_CHECK(xFileSysInit());
	xFileSysTestFSdir("/A");
	xFileSysTestFSdir("/A/1");
	xFileSysTestFSdir("/A/1/2");
	xFileSysTestFSdir("/A/1/2/3");
	xFileSysTestFSdir("/b");
	xFileSysTestFSdir("/b/1");
	xFileSysTestFSdir("/b/1/2");
	xFileSysTestFSdir("/b/1/2/3");
	xFileSysTestFSdir("/c");
	xFileSysTestFSdir("/c/1");
	xFileSysTestFSdir("/c/1/2");
	xFileSysTestFSdir("/c/1/2/3");
	xFileSysListDirTree(&sRprt, "/c/1/2/3");
	xFileSysListDirTree(&sRprt, "/c");
	xFileSysListDirTree(&sRprt, "/A/1/2");
	xFileSysListDirTree(&sRprt, "");
	xFileSysTestFSdelete("/c/1/2/3");
	xFileSysTestFSdelete("/c/1/2");
	xFileSysTestFSdelete("/c/1");
	xFileSysTestFSdelete("/c");
	xFileSysTestFSdelete("/b/1/2/3");
	xFileSysTestFSdelete("/b/1/2");
	xFileSysTestFSdelete("/b/1");
	xFileSysTestFSdelete("/b");
	xFileSysTestFSdelete("/A/1/2/3");
	xFileSysTestFSdelete("/A/1/2");
	xFileSysTestFSdelete("/A/1");
	xFileSysTestFSdelete("/A");
	xFileSysListPartition(&sRprt);
	ESP_ERROR_CHECK(xFileSysDeInit());
}
