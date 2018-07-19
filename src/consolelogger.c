// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <tchar.h>
#include "azure_c_shared_utility/xlogging.h"
#include "azure_c_shared_utility/consolelogger.h"

#if (defined(_MSC_VER)) && (!(defined WINCE))
#include "windows.h"

wchar_t *  to_wchar(const char * strA)
{
    wchar_t*  resultW;
    if (strA == NULL)
    {
        return NULL;
    }
    DWORD dwNum = MultiByteToWideChar(CP_ACP, 0, strA, -1, NULL, 0);
    resultW = (wchar_t*)malloc(dwNum * sizeof(wchar_t));
    if (resultW == NULL)
    {
        return NULL;
    }
    MultiByteToWideChar(CP_ACP, 0, strA, -1, resultW, dwNum);

    return resultW;
}

/*returns a string as if printed by vprintf*/
static TCHAR* vprintf_alloc(const TCHAR* format, va_list va)
{
    TCHAR* result;
    int neededSize = _vsntprintf(NULL, 0, format, va);
    if (neededSize < 0)
    {
        result = NULL;
    }
    else
    {
        result = (TCHAR*)malloc(neededSize + 1);
        if (result == NULL)
        {
            /*return as is*/
        }
        else
        {
            if (_vsntprintf(result, neededSize + 1, format, va) != neededSize)
            {
                free(result);
                result = NULL;
            }
        }
    }
    return result;
}

/*returns a string as if printed by printf*/
static TCHAR* printf_alloc(const TCHAR* format, ...)
{
    TCHAR* result;
    va_list va;
    va_start(va, format);
    result = vprintf_alloc(format, va);
    va_end(va);
    return result;
}

/*returns NULL if it fails*/
static TCHAR* lastErrorToString(DWORD lastError)
{
    TCHAR* result;
    if (lastError == 0)
    {
        result = printf_alloc(_T("")); /*no error should appear*/
        if (result == NULL)
        {
            (void)_tprintf(_T("failure in printf_alloc"));
        }
        else
        {
            /*return as is*/
        }
    }
    else
    {
        TCHAR temp[MESSAGE_BUFFER_SIZE];
        if (FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, lastError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), temp, MESSAGE_BUFFER_SIZE, NULL) == 0)
        {
            result = printf_alloc(_T("GetLastError()=0X%x"), lastError);
            if (result == NULL)
            {
                (void)_tprintf(_T("failure in printf_alloc\n"));
                /*return as is*/
            }
            else
            {
                /*return as is*/
            }
        }
        else
        {
            /*eliminate the \r or \n from the string*/
            /*one replace of each is enough*/
            TCHAR* whereAreThey;
            if ((whereAreThey = _tcschr(temp, '\r')) != NULL)
            {
                *whereAreThey = '\0';
            }
            if ((whereAreThey = _tcschr(temp, '\n')) != NULL)
            {
                *whereAreThey = '\0';
            }

            result = printf_alloc(_T("GetLastError()==0X%x (%s)"), lastError, temp);

            if (result == NULL)
            {
                (void)_tprintf(_T("failure in printf_alloc\n"));
                /*return as is*/
            }
            else
            {
                /*return as is*/
            }
        }
    }
    return result;
}
/*this function will use 1x printf (in the happy case) .*/
/*more than 1x printf / function call can mean intermingled LogErrors in a multithreaded env*/
/*the function will also attempt to produce some human readable strings for GetLastError*/
void consolelogger_log_with_GetLastError(const char* file, const char* func, int line, const char* format, ...)
{
	DWORD lastError;
	TCHAR* lastErrorAsString;
	int lastErrorAsString_should_be_freed;
	time_t t;
    int systemMessage_should_be_freed;
	TCHAR* systemMessage;
    int userMessage_should_be_freed;
	TCHAR* userMessage;

    va_list args;
    wchar_t * formatw = to_wchar(format);
    va_start(args, formatw);

    /*this is what this case will do:
    1. snip the last error
    2. create a string with what that last error means
    3. printf the system message (__FILE__, __LINE__ etc) + the last error + whatever the user wanted
    */
    /*1. snip the last error*/
    lastError = GetLastError();

    /*2. create a string with what that last error means*/
    lastErrorAsString = lastErrorToString(lastError);
    if (lastErrorAsString == NULL)
    {
        (void)_tprintf(_T("failure in lastErrorToString"));
        lastErrorAsString = _T("");
        lastErrorAsString_should_be_freed = 0;
    }
    else
    {
        lastErrorAsString_should_be_freed = 1;
    }

    _tctime(&t);
    systemMessage = printf_alloc(_T("Error: Time:%.24s File:%S Func:%S Line:%d %s"), _tctime(&t), file, func, line, lastErrorAsString);

    if (systemMessage == NULL)
    {
        systemMessage = _T("");
        (void)_tprintf(_T("Error: [FAILED] Time:%.24s File : %S Func : %S Line : %d %s"), _tctime(&t), file, func, line, lastErrorAsString);
        systemMessage_should_be_freed = 0;
    }
    else
    {
        systemMessage_should_be_freed = 1;
    }

    userMessage = vprintf_alloc(formatw, args);
    if (userMessage == NULL)
    {
        (void)_tprintf(_T("[FAILED] "));
        (void)_vtprintf(formatw, args);
        (void)_tprintf(_T("\n"));
        userMessage_should_be_freed = 0;
    }
    else
    {
        /*3. printf the system message(__FILE__, __LINE__ etc) + the last error + whatever the user wanted*/
        (void)_tprintf(_T("%s %s\n"), systemMessage, userMessage);
        userMessage_should_be_freed = 1;
    }

    if (userMessage_should_be_freed == 1)
    {
        free(userMessage);
    }

    if (systemMessage_should_be_freed == 1)
    {
        free(systemMessage);
    }

    if (lastErrorAsString_should_be_freed == 1)
    {
        free(lastErrorAsString);
    }
    if (formatw)
    {
        free(formatw);
    }
    va_end(args);
}
#endif

#if defined(__GNUC__)
__attribute__ ((format (printf, 6, 7)))
#endif
void consolelogger_log(LOG_CATEGORY log_category, const char* file, const char* func, int line, unsigned int options, const char* format, ...)
{
    time_t t;
    va_list args;

    wchar_t * formatw = to_wchar(format);
    
    va_start(args, formatw);

    t = time(NULL);

    switch (log_category)
    {
    case AZ_LOG_INFO:
        (void)_tprintf(_T("Info: "));
        break;
    case AZ_LOG_ERROR:
        (void)_tprintf(_T("Error: Time:%.24s File:%S Func:%S Line:%d "), _tctime(&t), file, func, line);
        break;
    default:
        break;
    }

    (void)_vtprintf(formatw, args);
    va_end(args);

    (void)log_category;
    if (options & LOG_LINE)
    {
        (void)_tprintf(_T("\r\n"));
    }

    if (formatw)
    {
        free(formatw);
    }
}


