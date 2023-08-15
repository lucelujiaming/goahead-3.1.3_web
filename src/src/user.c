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
    trace(2, "get_last_modify_time path = %s", path);
    if(stat(path,&stat_buf)==0)
    {
        time_t last_modify_time=stat_buf.st_mtime;
        trace(2, "path = %s", path);
        trace(2, "last_modify_time = %ld", last_modify_time);
	sprintf(time_str, "%ld", last_modify_time);
        return ctime(&last_modify_time);
    }
    else
    {
	sprintf(time_str, "Unknown");
        trace(2, "path error");
    }
    return NULL;
}

#define  APP_PATH_LEN    128
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
    ret_str = (char *)malloc(1024);
    if(ret_str)
    {
        sprintf(ret_str, "{ \"version\": \"%s\", \"buildtime\": \"%s\", \"status\": \"%s\" }", 
           version_string, filetime_string, status_string);
    }
#endif
    trace(2, "ret_str = %s", ret_str);
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
    trace(2, "statusProc::pVal = %s", pMode);

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
        trace(2, "statusProc::pValue = %s", pValue);
        websDone(wp);
        iValue = atoi(pValue);
        if(iValue > 1 && iValue < 255)
        {
            sprintf(info_str, "/data/svm/www/change_ip_address.sh %s &", pValue);
            system(info_str);
        }
    }
    else if(strcmp(pMode, "set_datetime") == 0)
    {
        memset(info_str, 0x00, 1024);
        pValue = websGetVar(wp, "value", "");
        trace(2, "statusProc::pValue = %s", pValue);
        websDone(wp);
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
    else
    {
        websDone(wp);
    }
}

void sumbitProc(Webs *wp)
{
    
}

int check_uploadfile(Webs *wp)
{
    char app_sab_str[APP_PATH_LEN] = "";
    char app_scode_str[APP_PATH_LEN] = "";
    struct stat stat_buf;
    char *pVal;

    pVal = websGetVar(wp, "UPLOAD_DIR", "/data");
    trace(2, "[%s:%s:%d] pVal = %s", __FILE__, __FUNCTION__, __LINE__, pVal);

    sprintf(app_sab_str, "%s/app.sab", pVal);
    sprintf(app_scode_str, "%s/app.scode", pVal);
    if(stat(app_sab_str, &stat_buf)==0)
    {
        system("/data/svm/www/stop_svm.sh &");
        memset(app_sab_str, 0x00, APP_PATH_LEN);
        sprintf(app_sab_str, "mv %s/app.sab /data/svm/", pVal);
        system(app_sab_str);
        system("chmod 664 /data/svm/app.sab");
        system("/etc/rc.d/rc.svm &");
        trace(2, "uploadProc :: update by %s", app_sab_str);
    }
    else if(stat(app_scode_str, &stat_buf)==0)
    {
        system("/data/svm/www/stop_svm.sh &");
        memset(app_scode_str, 0x00, APP_PATH_LEN);
        sprintf(app_scode_str, "mv %s/app.scode /data/svm/", pVal);
        system(app_scode_str);
        system("chmod 664 /data/svm/app.scode");
        system("/etc/rc.d/rc.svm &");
        trace(2, "uploadProc :: update by %s", app_scode_str);
    }
    else
    {
        trace(2, "uploadProc :: update error");
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

