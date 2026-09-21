/*******************************************************************/
/*                                                                 */
/* Copyright 2007 Adobe                                            */
/* All Rights Reserved.                                            */
/*                                                                 */
/* NOTICE:  Adobe permits you to use, modify, and distribute this  */
/* file in accordance with the terms of the Adobe license          */
/* agreement accompanying it.                                      */
/*                                                                 */
/*******************************************************************/

#include "AEPinyinSearch.h"

typedef struct
{
    unsigned long index;
    char str[256];
} TableString;

TableString g_strs[StrID_NUMTYPES] = {
    StrID_NONE,
    "",
    StrID_Name,
    "拼音搜索",
    StrID_Description,
    "按拼音 / 首字母搜索效果与预设，回车应用到选中图层。",
    StrID_GenericError,
    "Error enabling AEPinyinSearch."

};

A_char* GetStringPtr(int strNum)
{
    return g_strs[strNum].str;
}
