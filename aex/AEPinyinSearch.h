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

#include "AEConfig.h"
#ifdef AE_OS_WIN
    #include <windows.h>
#endif

#include "entry.h"
#include "AE_GeneralPlug.h"
#include "AE_GeneralPlugPanels.h"
#include "AE_Macros.h"
#include "AEGP_SuiteHandler.h"
#ifndef DEBUG
    #define DEBUG
#endif
#include "SuiteHelper.h"
#include "SimpleSuiteHelper.h"
#include "String_Utils.h"
#include "AE_EffectSuites.h"
#include "PinyinPopup.h"
#include "PT_Err.h"

typedef enum
{
    StrID_NONE,
    StrID_Name,
    StrID_Description,
    StrID_GenericError,
    StrID_NUMTYPES
} StrIDType;

// This entry point is exported through the PiPL (.r file)
extern "C" DllExport AEGP_PluginInitFuncPrototype EntryPointFunc;