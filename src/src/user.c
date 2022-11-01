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

void sumbitProc(Webs *wp)
{
    
}

void statusProc(Webs *wp)
{
    char *pVal;

    pVal = websGetVar(wp, "mode", "");
    trace(2, "pVal = %s", pVal);
}

void UserDefineInitialize()
{
    websDefineAction("status", statusProc);
    websDefineAction("sumbitProc", sumbitProc);
}

void UserDefineDeinitialize()
{
    
}

