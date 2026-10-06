// filesys.h

#pragma once

#include "report.h"
#include <dirent.h>

#ifdef __cplusplus
extern "C" {
#endif

// ########################################## Structures ###########################################

#define	filesysBUFSIZE			0x1000


extern SemaphoreHandle_t shLFSmux;

// ################################## Global/public variables ######################################

/**
 * @brief
 * @return
 */
int	xFileSysInit(void);

/**
 * @brief
 * @return
 */
int	xFileSysDeInit(void);

/**
 * @brief	Raw partition numbers - the report-free half of xFileSysListPartition()
 * @return	ESP_OK or the esp_littlefs_info() error
 */
int xFileSysInfo(size_t * pTotal, size_t * pUsed);

/**
 * @brief	Is the partition currently mounted?
 */
bool xFileSysMounted(void);

/**
 * @brief	ERASE and re-create the file system on the partition (esp_littlefs_format)
 * @return	ESP_OK or error
 */
int xFileSysFormat(void);

/**
 * @brief	Used by Syslog to persist host messages if not connected, hence no error logging.
 * @return	ErSUCCESS or error code
*/

int xFileSysFileWrite(const char * pFName, int fMode, char * pData, ssize_t sSize);
int xFileSysFileWriteS(const char * pFName, int fMode, char * pData);

/**
 * @brief		calculate size of file specified
 * @param[in]	pccFname - name of file
 * @return		file size (0+ non negative value) or erFAILURE
 */
ssize_t xFileSysGetFileSize(const char * pccFname);

/**
 * @brief
 * @param[in]   psR pointer to formatting report structure
 * @param[in]
 * @return
 */
int xFileSysListFileContent(report_t * psR, FILE * fp);

/**
 * @brief
 * @param[in]   psR pointer to formatting report structure
 * @return
 */
int xFileSysFileDisplay(report_t * psR, const char * pccFN);

/**
 * @brief
 * @param[in]   psR pointer to formatting report structure
 * @param[in]   pccDir top level directory path
 * @return
 */
int xFileSysListFileInfo(report_t * psR, struct dirent * psDE, const char * pccDir);

/**
 * @brief   Recursively list a directory tree, optionally the files and list content as well
 * @param[in]   psR pointer to formatting report structure
 * @param[in]   pccDir top level directory path
 * @return
 */
int xFileSysListDirTree(report_t * psR, const char * pccDir);

/**
 * @brief
 * @param[in]   psR pointer to formatting report structure
 * @return
 */
int xFileSysListPartition(report_t * psR);

/**
 * @brief	write a block of memory to specified file
 * @param	pcFname - filename to create & write into
 * @param	xAddr - memory address to start writing from
 * @param	xSize - size of memory to write to file
 * @return	ESP_OK / erSUCCESS, erFAILURE or similar
*/
int xFileSysMemoryToFile(const char * pcFName, u32_t xAddr, size_t xSize);

/**
 * @brief	write file to memory
 * @param	pcFname - filename to read from
 * @param	xAddr - memory address to writing to
 * @return	ESP_OK / erSUCCESS, erFAILURE or similar
*/
int xFileSysFileToMemory(const char * pcFName, u32_t xAddr);

void xFileSysTestFS(void);


#ifdef __cplusplus
}
#endif
