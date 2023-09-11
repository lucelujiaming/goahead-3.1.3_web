/********************************* Includes ***********************************/
#include "goahead.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <locale.h>
#include <sys/wait.h>
#include <unistd.h>

#include    "cJSON.h"

// Fireware decode macro and struct
#define FIRMWARE_MAGIC 0x55005678

#define FIRMWARE_SYSLOADER_KEY  's'
#define FIRMWARE_UBOOT_KEY      'u'
#define FIRMWARE_KERNEL_KEY     'k'
#define FIRMWARE_ROOTFS_KEY     'r'

#define FIRMWARE_SYSLOADER_FILENAME  "sramloader.bin"
#define FIRMWARE_UBOOT_FILENAME      "u-boot.bin"
#define FIRMWARE_KERNEL_FILENAME     "uImage"
#define FIRMWARE_ROOTFS_FILENAME     "rootfs.cramfs"

typedef struct {
    unsigned int magic;
    unsigned int len;
    unsigned short s_sum;
    unsigned short u_sum;
    unsigned short k_sum;
    unsigned short r_sum;
    unsigned int s_len;
    unsigned int u_len;
    unsigned int k_len;
    unsigned int r_len;
    char version[16];
    char btime[16];
} FIRMWARE_T;
// Fireware decode macro and struct end

static int get_cmd_printf(char *cmd, char *buf, int bufSize)
{
    FILE *fp;
    int status;
    char *p;
    char buffer[1024] = {0};
    int bufferSize = sizeof(buffer);
    int length = 0;
    int writeLen = 0;

    if (cmd == NULL || buf == NULL || bufSize <= 1)
        return -1;

    if ((fp = popen(cmd, "r")) == NULL) {
        return -1;
    }

    p = buffer;
    memset(buf, 0, bufSize);
    fgets(buffer, bufferSize, fp);
    do {
        writeLen = ((length + strlen(buffer)) > (bufSize - 1)) ? (bufSize - 1 - length) : strlen(buffer);

        memcpy(buf + length, buffer, writeLen);
        length += writeLen;
    } while (fgets(buffer, bufferSize, fp) != NULL);


    if (*(p = &buf[length - 1]) == 0x0A)
        *p = 0;

    status = pclose(fp);
    if (WIFEXITED(status)) {

        return WEXITSTATUS(status);
    }

    return -1;
}

char* get_last_modify_time(char* path, char* time_str)
{
    struct stat stat_buf;
    trace(2, "[%s:%s:%d] get_last_modify_time path = %s", __FILE__, __FUNCTION__, __LINE__, path);
    if(stat(path,&stat_buf)==0)
    {
        time_t last_modify_time=stat_buf.st_mtime;
        trace(2, "[%s:%s:%d] path = %s and last_modify_time = %ld"
			, __FILE__, __FUNCTION__, __LINE__, path, last_modify_time);
        sprintf(time_str, "%ld", last_modify_time);
        return ctime(&last_modify_time);
    }
    else
    {
        sprintf(time_str, "Unknown");
        trace(2, "[%s:%s:%d] path error", __FILE__, __FUNCTION__, __LINE__);
    }
    return NULL;
}

#define  APP_PATH_LEN    128
int get_ver_from_cmdline(char* cmd_line, char * ver_info)
{
    char *ver_start_pos = strstr(cmd_line, "ver=");
    if (ver_start_pos)
    {
        char *ver_end_pos = strchr(ver_start_pos + strlen("ver="), ' ');
        if (ver_end_pos)
        {
            strncpy(ver_info, ver_start_pos + strlen("ver="), 
                ver_end_pos - ver_start_pos - strlen("ver="));
            return 1;
        }
        else
        {
            strcpy(ver_info, ver_start_pos + strlen("ver="));
            return 1;
        }
    }
    return 0;
}

int get_uboot_version(char * ver_info)
{
    char cmdline_string[APP_PATH_LEN] = "";
    memset(ver_info, 0x00, APP_PATH_LEN);
    get_cmd_printf("cat /proc/cmdline", cmdline_string, APP_PATH_LEN);
    
    return get_ver_from_cmdline(cmdline_string, ver_info);
}

int get_kernel_version(char * ver_info)
{
    char kernel_name_string[APP_PATH_LEN] = "";
    memset(kernel_name_string, 0x00, APP_PATH_LEN);
    get_cmd_printf("uname -s", kernel_name_string, APP_PATH_LEN);
    
    char kernel_release_string[APP_PATH_LEN] = "";
    memset(kernel_release_string, 0x00, APP_PATH_LEN);
    get_cmd_printf("uname -r", kernel_release_string, APP_PATH_LEN);
    
    char kernel_version_string[APP_PATH_LEN] = "";
    memset(kernel_version_string, 0x00, APP_PATH_LEN);
    get_cmd_printf("uname -v", kernel_version_string, APP_PATH_LEN);
    
    sprintf(ver_info, "%s %s %s", kernel_name_string, kernel_release_string, kernel_version_string);
    return 1; 
}

int get_machine_name(char * machine_name_info)
{
    get_cmd_printf("uname -m", machine_name_info, APP_PATH_LEN);
    return 1; 
}

char* initProc()
{
    char *pVal;
    char *ret_str;
#ifdef USE_CJSON
    cJSON *ret_data;
    char info_str[1024];

    ret_data = cJSON_CreateObject();

    memset(info_str, 0x00, 1024);
    get_cmd_printf("cat /etc/VERSION", info_str, 1024);
    cJSON_AddStringToObject(ret_data, "version", info_str);

    memset(info_str, 0x00, 1024);
    pVal = get_last_modify_time("/data/svm/app.sab", info_str);
    if(pVal)
        cJSON_AddStringToObject(ret_data, "buildtime", info_str);
    else
        cJSON_AddStringToObject(ret_data, "buildtime", "Unknown");

    memset(info_str, 0x00, 1024);
    get_cmd_printf("ps | grep 'app.scode' | grep -v grep", info_str, 1024);
    if(strlen(info_str) >= strlen("app.scode"))
        cJSON_AddStringToObject(ret_data, "status", "standby");
    else
        cJSON_AddStringToObject(ret_data, "status", "shutdown");

    ret_str = cJSON_Print(ret_data);
    cJSON_Delete(ret_data);
#else
    char version_string[APP_PATH_LEN] = "";
    char filetime_string[APP_PATH_LEN] = "";
    char status_string[APP_PATH_LEN] = "";

    char uboot_version[APP_PATH_LEN] = "";
    char kernel_version[APP_PATH_LEN] = "";
    char machine_name[APP_PATH_LEN] = "";

    get_cmd_printf("cat /etc/VERSION", version_string, APP_PATH_LEN);
    pVal = get_last_modify_time("/data/svm/app.sab", filetime_string);
    get_cmd_printf("ps | grep 'app.scode' | grep -v grep", status_string, APP_PATH_LEN);
    if(strlen(status_string) >= strlen("app.scode"))
    {
        strcpy(status_string, "standby");
    }
    else
    {
        strcpy(status_string, "shutdown");
    }

    int iRet = get_uboot_version(uboot_version);
    if(iRet == 0)
    {
        strcpy(uboot_version, "unknown");
    }
    iRet = get_kernel_version(kernel_version);
    if(iRet == 0)
    {
        strcpy(kernel_version, "unknown");
    }
    iRet = get_machine_name(machine_name);
    
    ret_str = (char *)malloc(1024);
    if(ret_str)
    {
        sprintf(ret_str, "{ \"version\": \"%s\", \"buildtime\": \"%s\", \"status\": \"%s\", \"uboot\": \"%s\", \"kernel\": \"%s\", \"machine\": \"%s\" }", 
           version_string, filetime_string, status_string, uboot_version, kernel_version, machine_name);
    }
#endif
    trace(2, "[%s:%s:%d] ret_str = %s", __FILE__, __FUNCTION__, __LINE__, ret_str);
    return ret_str;
}

void statusProc(Webs *wp)
{
    char *ret_str;
    char *pMode;
    char *pValue;
    int   iValue;
    char info_str[1024];

    pMode = websGetVar(wp, "mode", "");
    trace(2, "[%s:%s:%d] statusProc::pVal = %s", __FILE__, __FUNCTION__, __LINE__, pMode);

    if(strcmp(pMode, "init") == 0)
    {
        ret_str = initProc();
        if(ret_str)
        {
            websWrite(wp, ret_str);
            free(ret_str);
        }
        websDone(wp);
    }
    else if(strcmp(pMode, "set_ipaddress") == 0)
    {
        pValue = websGetVar(wp, "value", "");
        trace(2, "[%s:%s:%d] statusProc::pValue = %s", __FILE__, __FUNCTION__, __LINE__, pValue);
        websDone(wp);
        iValue = atoi(pValue);
        if(iValue > 1 && iValue < 255)
        {
            sprintf(info_str, "/data/svm/www/change_ip_address.sh %s &", pValue);
            system(info_str);
            // change_ip_address.sh would restart goahead, so we need not return websDone
        }
    }
    else if(strcmp(pMode, "set_macaddress") == 0)
    {
        pValue = websGetVar(wp, "value", "");
        trace(2, "[%s:%s:%d] statusProc::pValue = %s", __FILE__, __FUNCTION__, __LINE__, pValue);
        websDone(wp);
        
        // if(strlen(pValue) == 17)
        sprintf(info_str, "/data/svm/www/change_mac_address.sh %s &", pValue);
        system(info_str);
        // change_ip_address.sh would restart goahead, so we need not return websDone
        websDone(wp);
    }
    else if(strcmp(pMode, "set_datetime") == 0)
    {
        pValue = websGetVar(wp, "value", "");
        trace(2, "[%s:%s:%d] statusProc::pValue = %s", __FILE__, __FUNCTION__, __LINE__, pValue);
        websDone(wp);
        memset(info_str, 0x00, 1024);
        sprintf(info_str, "date -s %s &", pValue);
        system(info_str);
    }
    else if(strcmp(pMode, "get_iprange") == 0)
    {
        memset(info_str, 0x00, 1024);
        get_cmd_printf("cat /data/svm/current_iprange", info_str, 1024);
        websWrite(wp, info_str);
        websDone(wp);
    }
    else if(strcmp(pMode, "get_ipconfig") == 0)
    {
        memset(info_str, 0x00, 1024);
        get_cmd_printf("cat /data/svm/current_ip", info_str, 1024);
        // Use default value
        if(strlen(info_str) == 0)
        {
            strcpy(info_str, "2");
        }
        websWrite(wp, info_str);
        websDone(wp);
    }
    else if(strcmp(pMode, "get_macconfig") == 0)
    {
        memset(info_str, 0x00, 1024);
        get_cmd_printf("cat /data/svm/current_mac", info_str, 1024);
        websWrite(wp, info_str);
        websDone(wp);
    }
    else
    {
        websDone(wp);
    }
}

void sumbitProc(Webs *wp)
{
    
}

int calc_align(int value , int align)
{
    return (value + align -1) & (~(align -1));
}

static unsigned short gen_sum(unsigned char key, char * file_pos, unsigned int filelen)
{
    unsigned char key1 = key;
    unsigned char key2 = key;

    unsigned int i;

    if (filelen == 0)
        return 0;

    for(i=0; i<filelen; i++) {
        if (file_pos[i] == (char)key) {
            key2 += 1;
        } else {
            key1 ^= file_pos[i];
        }
    }
    return (unsigned short)key1 + ((unsigned short)key2 << 8);
}

int decode_firmware(char *cFileName, char *outputPath)
{
    int iFileLen = 0, iFileAlignLen = 0;
    struct stat st;

    char outFilePath[128];

    FIRMWARE_T firmware_head;

    if (lstat(cFileName, &st) < 0) {
        printf("lstat failed: %s\r\n", cFileName);
    	return 0;
    }
    iFileLen = st.st_size;
    iFileAlignLen = calc_align(iFileLen, 1024);
    
    char *buf = NULL;
    buf = (char *)malloc(iFileAlignLen);
    memset(buf, 0, iFileAlignLen);

    FILE * fp = fopen(cFileName, "r");
    if(fp < 0)
    {
        free(buf);
        printf("open failed: %s\r\n", cFileName);
    	return 0;
    }

    int n = fread(buf, sizeof(char), iFileLen, fp);

    memcpy(&firmware_head, buf, sizeof(FIRMWARE_T));
    if(firmware_head.magic != FIRMWARE_MAGIC)
    {
        free(buf);
        fclose(fp);
        printf("Error FIRMWARE_MAGIC\n");
    	return 0;
    }
    // Check sysloader sum
    unsigned short uCheckSum = gen_sum(FIRMWARE_SYSLOADER_KEY, 
                    buf + sizeof(FIRMWARE_T), 
                    firmware_head.s_len);
    if(uCheckSum != firmware_head.s_sum)
    {
        free(buf);
        fclose(fp);
        printf("Error s_num\n");
    	return 0;
    }

    // Check u-boot sum
    uCheckSum = gen_sum(FIRMWARE_UBOOT_KEY, 
                    buf + sizeof(FIRMWARE_T) + firmware_head.s_len, 
                    firmware_head.u_len);
    if(uCheckSum != firmware_head.u_sum)
    {
        free(buf);
        fclose(fp);
        printf("Error u_num\n");
    	return 0;
    }

    // Check kernel sum
    uCheckSum = gen_sum(FIRMWARE_KERNEL_KEY, 
                    buf + sizeof(FIRMWARE_T) + firmware_head.s_len + firmware_head.u_len, 
                    firmware_head.k_len);
    if(uCheckSum != firmware_head.k_sum)
    {
        free(buf);
        fclose(fp);
        printf("Error k_num\n");
    	return 0;
    }

    // Check rootfs sum
    uCheckSum = gen_sum(FIRMWARE_ROOTFS_KEY, 
                    buf + sizeof(FIRMWARE_T) + firmware_head.s_len + firmware_head.u_len + firmware_head.k_len, 
                    firmware_head.r_len);
    if(uCheckSum != firmware_head.r_sum)
    {
        free(buf);
        fclose(fp);
        printf("Error r_num\n");
    	return 0;
    }

    // unpacked sysloader
    sprintf(outFilePath, "%s/%s", outputPath, FIRMWARE_SYSLOADER_FILENAME);
    FILE * fpSysloader = fopen(outFilePath, "w");
    if(fpSysloader < 0)
    {
        free(buf);
        fclose(fp);
        printf("open failed: %s\r\n", FIRMWARE_SYSLOADER_FILENAME);
        return 0;
    }
    fwrite(buf + sizeof(FIRMWARE_T), sizeof(char), firmware_head.s_len, fpSysloader);
    fclose(fpSysloader);
    printf("Unpack success: %s\r\n", FIRMWARE_SYSLOADER_FILENAME);

    // unpacked u-boot 
    sprintf(outFilePath, "%s/%s", outputPath, FIRMWARE_UBOOT_FILENAME);
    FILE * fpUboot = fopen(outFilePath, "w");
    if(fp < 0)
    {
        free(buf);
        fclose(fp);
        printf("open failed: %s\r\n", FIRMWARE_UBOOT_FILENAME);
        return 0;
    }
    fwrite(buf + sizeof(FIRMWARE_T) + firmware_head.s_len, sizeof(char), firmware_head.u_len, fpUboot);
    fclose(fpUboot);
    printf("Unpack success: %s\r\n", FIRMWARE_UBOOT_FILENAME);

    // unpacked kernel 
    sprintf(outFilePath, "%s/%s", outputPath, FIRMWARE_KERNEL_FILENAME);
    FILE * fpKernel = fopen(outFilePath, "w");
    if(fpKernel < 0)
    {
        free(buf);
        fclose(fp);
        printf("open failed: %s\r\n", FIRMWARE_KERNEL_FILENAME);
        return 0;
    }
    fwrite(buf + sizeof(FIRMWARE_T) + firmware_head.s_len + firmware_head.u_len, 
                sizeof(char), firmware_head.k_len, fpKernel);
    fclose(fpKernel);
    printf("Unpack success: %s\r\n", FIRMWARE_KERNEL_FILENAME);

    // unpacked rootfs 
    sprintf(outFilePath, "%s/%s", outputPath, FIRMWARE_ROOTFS_FILENAME);
    FILE * fpRootfs = fopen(outFilePath, "w");
    if(fpRootfs < 0)
    {
        free(buf);
        fclose(fp);
        printf("open failed: %s\r\n", FIRMWARE_ROOTFS_FILENAME);
        return 0;
    }
    fwrite(buf + sizeof(FIRMWARE_T) + firmware_head.s_len + firmware_head.u_len + firmware_head.k_len, 
                sizeof(char), firmware_head.r_len, fpRootfs);
    fclose(fpRootfs);
    printf("Unpack success: %s\r\n", FIRMWARE_ROOTFS_FILENAME);

    free(buf);
    fclose(fp);
    return 1;
}

int check_uploadfile(Webs *wp)
{
    char            key[64];
	
    char app_sab_str[APP_PATH_LEN] = "";
    char app_scode_str[APP_PATH_LEN] = "";
    char app_firmware_str[APP_PATH_LEN] = "";
    struct stat stat_buf;
    char *pPathVal;
    char *pFileNameVal;

    pPathVal = websGetVar(wp, "UPLOAD_DIR", "/data/upload");
    trace(2, "[%s:%s:%d] pVal = %s", __FILE__, __FUNCTION__, __LINE__, pPathVal);

    sprintf(app_sab_str, "%s/app.sab", pPathVal);
    sprintf(app_scode_str, "%s/app.scode", pPathVal);
	// Get filename
    fmt(key, sizeof(key), "FILE_CLIENT_FILENAME_%s", wp->uploadVar);
    pFileNameVal = websGetVar(wp, key, "");
    char *filename_start_pos = strstr(pFileNameVal, "firmware_V");
    if (filename_start_pos)
    {
    	sprintf(app_firmware_str, "%s/%s", pPathVal, pFileNameVal);
    }
	
    if(stat(app_sab_str, &stat_buf)==0)
    {
        memset(app_sab_str, 0x00, APP_PATH_LEN);
        sprintf(app_sab_str, "mv %s/app.sab /data/svm/", pPathVal);
        system(app_sab_str);
        system("chmod 664 /data/svm/app.sab");
        // Stop svm and watch dog would restart svm
        system("/data/svm/www/stop_svm.sh &");
        // system("/etc/rc.d/rc.svm &");
        trace(2, "[%s:%s:%d] uploadProc :: update by %s", __FILE__, __FUNCTION__, __LINE__, app_sab_str);
    }
    else if(stat(app_scode_str, &stat_buf)==0)
    {
        memset(app_scode_str, 0x00, APP_PATH_LEN);
        sprintf(app_scode_str, "mv %s/app.scode /data/svm/", pPathVal);
        system(app_scode_str);
        system("chmod 664 /data/svm/app.scode");
        // Stop svm and watch dog would restart svm
        system("/data/svm/www/stop_svm.sh &");
        // system("/etc/rc.d/rc.svm &");
        trace(2, "[%s:%s:%d] uploadProc :: update by %s", __FILE__, __FUNCTION__, __LINE__, app_scode_str);
    }
    else if(stat(app_firmware_str, &stat_buf)==0)
    {
        int iRet = decode_firmware(app_firmware_str, pPathVal);
        if(iRet == 1)
        {
            remove(app_firmware_str);
            trace(2, "[%s:%s:%d] uploadProc :: update and unpack by %s", 
                            __FILE__, __FUNCTION__, __LINE__, app_firmware_str);

            system("/data/svm/www/update_firmware.sh ");
            trace(2, "[%s:%s:%d] uploadProc :: update finish by %s (This should never print)", 
                            __FILE__, __FUNCTION__, __LINE__, app_firmware_str);
            
        }
        trace(2, "[%s:%s:%d] uploadProc :: update %s error", 
                            __FILE__, __FUNCTION__, __LINE__, app_firmware_str);
        return 1;
    }
    else
    {
        trace(2, "[%s:%s:%d] uploadProc :: update error", __FILE__, __FUNCTION__, __LINE__);
        return 1;
    }
    return 0;
}

void uploadProc(Webs *wp)
{
    char info_str[1024];
    int iRet = check_uploadfile(wp);
    sprintf(info_str, "%d", iRet);
    websWrite(wp, info_str);
    websDone(wp);
}

void upgradeProc(Webs *wp)
{
    char info_str[1024];
    int iRet = check_uploadfile(wp);
    sprintf(info_str, "%d", iRet);
    websWrite(wp, info_str);
    websDone(wp);
}

void UserDefineInitialize()
{
    websDefineAction("status", statusProc);

    websDefineAction("uploadProc", uploadProc);
    websDefineAction("upgrade", upgradeProc);
    websDefineAction("sumbitProc", sumbitProc);
}

void UserDefineDeinitialize()
{
    
}

