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
inline void AEPinyinLog(const char* fmt, ...)
{
    char dir[MAX_PATH] = {};
    if (GetTempPathA(MAX_PATH, dir) == 0)
    {
        return;
    }
    const std::string path = std::string(dir) + "AEPinyinSearch.log";

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
