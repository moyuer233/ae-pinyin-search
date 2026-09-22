/*******************************************************************/
/*                                                                 */
/* AE Pinyin Search - one-line file log                            */
/*                                                                 */
/* The plug-in used to fail completely silently: a class that would */
/* not register, a hotkey the OS refused, a hook that never went in */
/* all looked the same from the outside (nothing happens). This     */
/* writes ASCII lines to %TEMP%\AEPinyinSearch.log so the next      */
/* problem is diagnosable without guessing.                         */
/*                                                                 */
/*******************************************************************/

#ifndef AEPINYINSEARCH_DIAGLOG_H
#define AEPINYINSEARCH_DIAGLOG_H

#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <string>

// Messages must stay ASCII: the file is opened in text mode and is read by
// humans and by scripts that do not know the machine's code page.
//
// Not for low-level hook callbacks: the system waits for those and unhooks a
// callback that takes too long, so they only post a message and let the thread
// that owns the window write the line.
inline void AEPinyinLog(const char* fmt, ...)
{
    char dir[MAX_PATH] = {};
    if (GetTempPathA(MAX_PATH, dir) == 0)
    {
        return;
    }
    const std::string path = std::string(dir) + "AEPinyinSearch.log";

    // Lines are appended for the lifetime of every session, so start over once the
    // file gets big instead of letting it grow without bound.
    WIN32_FILE_ATTRIBUTE_DATA attr = {};
    if (GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &attr) &&
        (attr.nFileSizeHigh > 0 || attr.nFileSizeLow > (1024u * 1024u)))
    {
        DeleteFileA(path.c_str());
    }

    FILE* f = NULL;
    if (fopen_s(&f, path.c_str(), "a") != 0 || !f)
    {
        return;
    }

    SYSTEMTIME st = {};
    GetLocalTime(&st);
    std::fprintf(
        f, "%04d-%02d-%02d %02d:%02d:%02d  ", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute,
        st.wSecond);

    va_list args;
    va_start(args, fmt);
    std::vfprintf(f, fmt, args);
    va_end(args);

    std::fputc('\n', f);
    std::fclose(f);
}

#endif // AEPINYINSEARCH_DIAGLOG_H
