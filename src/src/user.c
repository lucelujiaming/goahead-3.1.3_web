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
        trace(2, "path error");
    return NULL;
}

void sumbitProc(Webs *wp)
{
    
}

void statusProc(Webs *wp)
{
    char *ret_str;
    char *pVal;
    cJSON *ret_data;
    char info_str[1024];

    pVal = websGetVar(wp, "mode", "");
    trace(2, "statusProc::pVal = %s", pVal);

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
    trace(2, "ret_str = %s", ret_str);
    websWrite(wp, ret_str);
    free(ret_str);
    websDone(wp);
}

void UserDefineInitialize()
{
    websDefineAction("status", statusProc);
    websDefineAction("sumbitProc", sumbitProc);
}

void UserDefineDeinitialize()
{
    
}

