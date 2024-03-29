/*
    upload.c -- File upload handler

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/*********************************** Includes *********************************/

#include    "goahead.h"

#if BIT_GOAHEAD_UPLOAD
/************************************ Locals **********************************/
/*
    Upload states
 */
#define UPLOAD_REQUEST_HEADER    1   /* Request header */
#define UPLOAD_BOUNDARY          2   /* Boundary divider */
#define UPLOAD_CONTENT_HEADER    3   /* Content part header */
#define UPLOAD_CONTENT_DATA      4   /* Content encoded data */
#define UPLOAD_CONTENT_END       5   /* End of multipart message */

static char *uploadDir;

/*********************************** Forwards *********************************/

static void defineUploadVars(Webs *wp);
static char *getBoundary(Webs *wp, char *buf, ssize bufLen);
static int initUpload(Webs *wp);
static int processContentBoundary(Webs *wp, char *line);
static int processContentData(Webs *wp);
static int processUploadHeader(Webs *wp, char *line);

/************************************* Code ***********************************/
/*
    The upload handler functions as a filter. It never actually handles a request
 */
static bool uploadHandler(Webs *wp)
{
    assert(websValid(wp));

    if (!(wp->flags & WEBS_UPLOAD)) {
        return 0;
    }
    return initUpload(wp);
}


static int initUpload(Webs *wp)
{
    char    *boundary;
    
    if (wp->uploadState == 0) {
        wp->uploadState = UPLOAD_BOUNDARY;
        if ((boundary = strstr(wp->contentType, "boundary=")) != 0) {
            boundary += 9;
            wp->boundary = sfmt("--%s", boundary);
            wp->boundaryLen = strlen(wp->boundary);
        }
        if (wp->boundaryLen == 0 || *wp->boundary == '\0') {
            websError(wp, HTTP_CODE_BAD_REQUEST, "Bad boundary");
            return -1;
        }
        websSetVar(wp, "UPLOAD_DIR", uploadDir);
        wp->files = hashCreate(11);
    }
    return 0;
}


static void freeUploadFile(WebsUpload *up)
{
    if (up->filename) {
        unlink(up->filename);
        wfree(up->filename);
    }
    wfree(up->clientFilename);
    wfree(up->contentType);
    wfree(up);
}


PUBLIC void websFreeUpload(Webs *wp)
{
    WebsUpload  *up;
    WebsKey     *s;

    for (s = hashFirst(wp->files); s; s = hashNext(wp->files, s)) {
        up = s->content.value.symbol;
        freeUploadFile(up);
        if (up == wp->currentFile) {
            wp->currentFile = 0;
        }
    }
    hashFree(wp->files);
    if (wp->currentFile) {
        freeUploadFile(wp->currentFile);
        wp->currentFile = 0;
    }
    if (wp->upfd >= 0) {
        close(wp->upfd);
        wp->upfd = -1;
    }
}


PUBLIC int websProcessUploadData(Webs *wp) 
{
    char    *line, *nextTok;
    ssize   len, nbytes;
    int     done, rc;
    
    for (done = 0, line = 0; !done; ) {
        if  (wp->uploadState == UPLOAD_BOUNDARY || wp->uploadState == UPLOAD_CONTENT_HEADER) {
            /*
                Parse the next input line
             */
            line = wp->input.servp;
            if ((nextTok = memchr(line, '\n', bufLen(&wp->input))) == 0) {
                /* Incomplete line */
                break;
            }
            *nextTok++ = '\0';
            nbytes = nextTok - line;
            websConsumeInput(wp, nbytes);
            strim(line, "\r", WEBS_TRIM_END);
            len = strlen(line);
            if (line[len - 1] == '\r') {
                line[len - 1] = '\0';
            }
        }
        switch (wp->uploadState) {
        case 0:
            if (initUpload(wp) < 0) {
                done++;
            }
            break;

        case UPLOAD_BOUNDARY:
            if (processContentBoundary(wp, line) < 0) {
                done++;
            }
            break;

        case UPLOAD_CONTENT_HEADER:
            if (processUploadHeader(wp, line) < 0) {
                done++;
            }
            break;

        case UPLOAD_CONTENT_DATA:
            if ((rc = processContentData(wp)) < 0) {
                done++;
            }
            if (bufLen(&wp->input) < wp->boundaryLen) {
                /*  Incomplete boundary - return to get more data */
                done++;
            }
            break;

        case UPLOAD_CONTENT_END:
            done++;
            break;
        }
    }
    if (!websValid(wp)) {
        return -1;
    }
    bufCompact(&wp->input);
    return 0;
}


static int processContentBoundary(Webs *wp, char *line)
{
    /*
        Expecting a multipart boundary string
     */
    if (strncmp(wp->boundary, line, wp->boundaryLen) != 0) {
        websError(wp, HTTP_CODE_BAD_REQUEST, "Bad upload state. Incomplete boundary");
        return -1;
    }
    if (line[wp->boundaryLen] && strcmp(&line[wp->boundaryLen], "--") == 0) {
        wp->uploadState = UPLOAD_CONTENT_END;
    } else {
        wp->uploadState = UPLOAD_CONTENT_HEADER;
    }
    return 0;
}


static int processUploadHeader(Webs *wp, char *line)
{
    WebsUpload  *file;
    char            *key, *headerTok, *rest, *nextPair, *value;

    if (line[0] == '\0') {
        wp->uploadState = UPLOAD_CONTENT_DATA;
        return 0;
    }
    trace(7, "Header line: %s", line);

    headerTok = line;
    stok(line, ": ", &rest);

    if (scaselesscmp(headerTok, "Content-Disposition") == 0) {
        /*  
            The content disposition header describes either a form variable or an uploaded file.
        
            Content-Disposition: form-data; name="field1"
            >>blank line
            Field Data
            ---boundary
     
            Content-Disposition: form-data; name="field1" filename="user.file"
            >>blank line
            File data
            ---boundary
         */
        key = rest;
        wp->uploadVar = wp->clientFilename = 0;
        while (key && stok(key, ";\r\n", &nextPair)) {

            key = strim(key, " ", WEBS_TRIM_BOTH);
            stok(key, "= ", &value);
            value = strim(value, "\"", WEBS_TRIM_BOTH);

            if (scaselesscmp(key, "form-data") == 0) {
                /* Nothing to do */

            } else if (scaselesscmp(key, "name") == 0) {
                wp->uploadVar = sclone(value);

            } else if (scaselesscmp(key, "filename") == 0) {
                if (wp->uploadVar == 0) {
                    websError(wp, HTTP_CODE_BAD_REQUEST, "Bad upload state. Missing name field");
                    return -1;
                }
                wp->clientFilename = sclone(value);
                /*  
                    Create the file to hold the uploaded data
                 */
                if ((wp->uploadTmp = websTempFile(uploadDir, "tmp")) == 0) {
                    websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, 
                        "Can't create upload temp file %s. Check upload temp dir %s", wp->uploadTmp, uploadDir);
                    return -1;
                }
                trace(2, "DBG - File upload of: %s stored as %s", wp->clientFilename, wp->uploadTmp);

                if ((wp->upfd = open(wp->uploadTmp, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0600)) < 0) {
                    websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "Can't open upload temp file %s", wp->uploadTmp);
                    return -1;
                }
                /*  
                    Create the files[id]
                 */
                file = wp->currentFile = walloc(sizeof(WebsUpload));
                memset(file, 0, sizeof(WebsUpload));
                file->clientFilename = sclone(wp->clientFilename);
                file->filename = sclone(wp->uploadTmp);
            }
            key = nextPair;
        }

    } else if (scaselesscmp(headerTok, "Content-Type") == 0) {
        if (wp->clientFilename) {
            trace(5, "Set files[%s][CONTENT_TYPE] = %s", wp->uploadVar, rest);
            wp->currentFile->contentType = sclone(rest);
        }
    }
    return 0;
}


static void defineUploadVars(Webs *wp)
{
    WebsUpload      *file;
    char            key[64];

    file = wp->currentFile;
    fmt(key, sizeof(key), "FILE_CLIENT_FILENAME_%s", wp->uploadVar);
	// trace(2, "[%s:%s:%d] set %s = %s", __FILE__, __FUNCTION__, __LINE__, key, file->clientFilename);
    websSetVar(wp, key, file->clientFilename);

    fmt(key, sizeof(key), "FILE_CONTENT_TYPE_%s", wp->uploadVar);
	// trace(2, "[%s:%s:%d] set %s = %s", __FILE__, __FUNCTION__, __LINE__, key, file->contentType);
    websSetVar(wp, key, file->contentType);

    fmt(key, sizeof(key), "FILE_FILENAME_%s", wp->uploadVar);
	// trace(2, "[%s:%s:%d] set %s = %s", __FILE__, __FUNCTION__, __LINE__, key, file->filename);
    websSetVar(wp, key, file->filename);

    fmt(key, sizeof(key), "FILE_SIZE_%s", wp->uploadVar);
	// trace(2, "[%s:%s:%d] set %s = %d", __FILE__, __FUNCTION__, __LINE__, key, (int)file->size);
    websSetVarFmt(wp, key, "%d", (int) file->size);
}


static int writeToFile(Webs *wp, char *data, ssize len)
{
    WebsUpload      *file;
    ssize           rc;

    file = wp->currentFile;

    if ((file->size + len) > BIT_GOAHEAD_LIMIT_UPLOAD) {
        websError(wp, HTTP_CODE_REQUEST_TOO_LARGE, "Uploaded file exceeds maximum %d", (int) BIT_GOAHEAD_LIMIT_UPLOAD);
        return -1;
    }
    // trace(2, "[%s:%s:%d] log output: %d ", __FILE__, __FUNCTION__, __LINE__, len);
    if (len > 0) {
        /*  
            File upload. Write the file data.
         */
        if ((rc = write(wp->upfd, data, (int) len)) != len) {
            websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "Can't write to upload temp file %s, rc %d", wp->uploadTmp, rc);
            return -1;
        }
        file->size += len;
        // trace(2, "uploadFilter: Wrote %d bytes to %s", len, wp->uploadTmp);
    }
    return 0;
}


static int processContentData(Webs *wp)
{
    WebsUpload  *file;
    WebsBuf         *content;
    ssize           size, nbytes;
    char            *data, *bp;
    char            cmd_string[1024] = "";

    content = &wp->input;
    file = wp->currentFile;

    size = bufLen(content);
    if (size < wp->boundaryLen) {
        /*  Incomplete boundary. Return and get more data */
        return 0;
    }

    if ((bp = getBoundary(wp, content->servp, size)) == 0) {
        // trace(2, "uploadFilter: Got boundary filename %x", wp->clientFilename);
        if (wp->clientFilename) {
            /*  
                No signature found yet. probably more data to come. Must handle split boundaries.
             */
            data = content->servp;
            nbytes = ((int) (content->endp - data)) - (wp->boundaryLen - 1);
            // trace(2, "[%s:%s:%d] log output: %d ", __FILE__, __FUNCTION__, __LINE__, nbytes);
            if (nbytes > 0 && writeToFile(wp, content->servp, nbytes) < 0) {
                return -1;
            }
            websConsumeInput(wp, nbytes);
            /* Get more data */
            return 0;
        }
    }
    data = content->servp;
    nbytes = (bp) ? (bp - data) : bufLen(content);

    if (nbytes > 0) {
        websConsumeInput(wp, nbytes);
        /*  
            This is the CRLF before the boundary
         */
        if (nbytes >= 2 && data[nbytes - 2] == '\r' && data[nbytes - 1] == '\n') {
            nbytes -= 2;
        }
        if (wp->clientFilename) {
            /*  
                Write the last bit of file data and add to the list of files and define environment variables
             */
            trace(2, "[%s:%s:%d] log output: %d ", __FILE__, __FUNCTION__, __LINE__, nbytes);
            if (writeToFile(wp, data, nbytes) < 0) {
                return -1;
            }
            hashEnter(wp->files, wp->uploadVar, valueSymbol(file), 0);
            defineUploadVars(wp);

        } else {
            /*  
                Normal string form data variables
             */
            data[nbytes] = '\0'; 
            trace(5, "uploadFilter: form[%s] = %s", wp->uploadVar, data);
            websDecodeUrl(wp->uploadVar, wp->uploadVar, -1);
            websDecodeUrl(data, data, -1);
            websSetVar(wp, wp->uploadVar, data);
        }
    }
    if (wp->clientFilename) {
        /*  
            Now have all the data (we've seen the boundary)
         */
        trace(2, "[%s:%s:%d] log output: %s ", __FILE__, __FUNCTION__, __LINE__, wp->uploadTmp);
        sprintf(cmd_string, "cp %s %s/%s", wp->uploadTmp, uploadDir, wp->clientFilename);
        trace(2, "[%s:%s:%d] log output: %s ", __FILE__, __FUNCTION__, __LINE__, cmd_string);
        close(wp->upfd);
        wp->upfd = -1;
        wp->clientFilename = 0;
        wfree(wp->uploadTmp);
        wp->uploadTmp = 0;

        system(cmd_string);
    }
    wp->uploadState = UPLOAD_BOUNDARY;
    return 0;
}


/*  
    Find the boundary signature in memory. Returns pointer to the first match.
 */ 
static char *getBoundary(Webs *wp, char *buf, ssize bufLen)
{
    char    *cp, *endp;
    char    first;

    assert(buf);

    first = *wp->boundary;
    cp = buf;
    if (bufLen < wp->boundaryLen) {
        return 0;
    }
    endp = cp + (bufLen - wp->boundaryLen) + 1;
    while (cp < endp) {
        cp = (char *) memchr(cp, first, endp - cp);
        if (!cp) {
            return 0;
        }
        if (memcmp(cp, wp->boundary, wp->boundaryLen) == 0) {
            return cp;
        }
        cp++;
    }
    return 0;
}



WebsUpload *websLookupUpload(Webs *wp, char *key)
{
    WebsKey     *sp;

    if (wp->files) {
        if ((sp = hashLookup(wp->files, key)) == 0) {
            return 0;
        }
        return sp->content.value.symbol;
    }
    return 0;
}


WebsHash websGetUpload(Webs *wp)
{
    return wp->files;
}


PUBLIC void websUploadOpen()
{
    uploadDir = BIT_GOAHEAD_UPLOAD_DIR;
    if (*uploadDir == '\0') {
#if BIT_WIN_LIKE
        uploadDir = getenv("TEMP");
#else
        uploadDir = "/tmp";
#endif
    }
    trace(4, "Upload directory is %s", uploadDir);
    websDefineHandler("upload", uploadHandler, 0, 0);
}

#endif

/*
    @copy   default

    Copyright (c) Embedthis Software LLC, 2003-2013. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis GoAhead open source license or you may acquire 
    a commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
