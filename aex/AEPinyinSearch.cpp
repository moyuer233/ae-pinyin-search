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

#include <new>
#include "AEPinyinSearch.h"
#include "DiagLog.h"
#include "EffectNames.h"

// The global mouse hook and the hotkey live on a thread this plug-in owns.
//
// A low-level hook callback is delivered on the thread that installed the hook,
// and the input system waits for it (LowLevelHooksTimeout, 25 s on this machine)
// if that thread is busy. Installing it on After Effects' main thread therefore
// means: whenever AE is busy - selecting a layer, editing a value - a mouse
// event can be held up. On our own thread, which always pumps, that can't
// happen; the callbacks only post a message to the window that lives on AE's
// main thread, and AE handles it when it gets around to it.
class AEPinyinSearch
{
public:
    SPBasicSuite* i_pica_basicP;
    AEGP_PluginID i_pluginID;

    AEGP_SuiteHandler i_sp;
    AEGP_Command i_command;
    PinyinPopup* i_popup;
    EffectNames i_effectNames; // what the host calls the installed effects

    HWND i_pumpWnd;      // AE main thread: receives the marshalled toggle
    HANDLE i_inputThread; // our thread: owns the hook + the hotkey
    DWORD i_inputThreadId;
    bool i_hotkeyWithShift;
    bool i_hotkeyRegistered;

    static AEPinyinSearch* s_instance;
    static const UINT kMsgToggle = WM_APP + 1;

    /// STATIC BINDERS
    static LRESULT CALLBACK S_PumpWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        AEPinyinSearch* self = reinterpret_cast<AEPinyinSearch*>(GetWindowLongPtrA(hWnd, GWLP_USERDATA));
        if (!self)
        {
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
        if (message == kMsgToggle)
        {
            // wParam carries the tick count from when the request was posted, so
            // "AE main thread was busy for X ms" shows up in the log.
            const DWORD posted = static_cast<DWORD>(wParam);
            const DWORD waited = GetTickCount() - posted;
            if (waited > 200)
            {
                AEPinyinLog("toggle: AE main thread took %lu ms to get to it", waited);
            }
            self->TogglePopup();
            return 0;
        }
        return DefWindowProcA(hWnd, message, wParam, lParam);
    }

    static LRESULT CALLBACK S_HotkeyWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        AEPinyinSearch* self = reinterpret_cast<AEPinyinSearch*>(GetWindowLongPtrA(hWnd, GWLP_USERDATA));
        if (self && message == WM_HOTKEY && wParam == 1)
        {
            // A global hotkey also fires while another app is in front; the
            // search bar belongs to AE, so ignore those.
            if (AEPinyinSearch::IsAEForeground())
            {
                AEPinyinLog("toggle: hotkey");
                self->ToggleFromAnyThread();
            }
            return 0;
        }
        return DefWindowProcA(hWnd, message, wParam, lParam);
    }

    // Mouse side buttons: a low-level hook lets XBUTTON1/2 summon the search bar
    // without any vendor driver remapping. This runs on the input thread.
    //
    // Only the press is swallowed, and that is enough: activation follows the
    // press (WM_MOUSEACTIVATE), so if the host never sees the press it will not
    // take the foreground back and the popup stays open. The release needs no
    // handling - a lone mouse-up does not activate anything.
    static LRESULT CALLBACK S_MouseProc(int nCode, WPARAM wParam, LPARAM lParam)
    {
        if (nCode >= 0 && wParam == WM_XBUTTONDOWN && s_instance && IsAEForeground())
        {
            const MSLLHOOKSTRUCT* ms = reinterpret_cast<const MSLLHOOKSTRUCT*>(lParam);
            const int which = static_cast<int>((ms->mouseData >> 16) & 0xFFFF);
            if (which == XBUTTON1 || which == XBUTTON2)
            {
                AEPinyinLog("toggle: mouse x-button");
                s_instance->ToggleFromAnyThread();
                return 1; // do not let the host see the press
            }
        }
        return CallNextHookEx(NULL, nCode, wParam, lParam);
    }

    static HMODULE OwnModule(const void* addressInThisDll)
    {
        HMODULE module = NULL;
        GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(addressInThisDll), &module);
        return module;
    }

    static bool IsAEForeground()
    {
        HWND foreground = GetForegroundWindow();
        DWORD pid = 0;
        if (foreground)
        {
            GetWindowThreadProcessId(foreground, &pid);
        }
        return pid == GetCurrentProcessId();
    }

    // Safe from any thread: the window belongs to AE's main thread.
    void ToggleFromAnyThread()
    {
        if (i_pumpWnd)
        {
            PostMessageA(i_pumpWnd, kMsgToggle, static_cast<WPARAM>(GetTickCount()), 0);
        }
    }

    // Must run on AE's main thread (it owns i_popup).
    void TogglePopup()
    {
        if (i_popup)
        {
            i_popup->Toggle();
        }
    }

    // The input thread: register the hook + the hotkey, then pump forever. Both
    // callbacks are then served by a thread that is never busy doing AE work.
    static DWORD WINAPI S_InputThread(LPVOID param)
    {
        AEPinyinSearch* self = reinterpret_cast<AEPinyinSearch*>(param);
        HMODULE module = OwnModule(reinterpret_cast<const void*>(&AEPinyinSearch::S_MouseProc));

        // --- global hotkey -------------------------------------------------
        static const char* kHotkeyClass = "AEPinyinSearchHotkeyWnd";
        WNDCLASSA wc = {};
        wc.lpfnWndProc = S_HotkeyWndProc;
        wc.hInstance = reinterpret_cast<HINSTANCE>(module);
        wc.lpszClassName = kHotkeyClass;
        if (!RegisterClassA(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        {
            AEPinyinLog("hotkey: RegisterClassA failed, err=%lu", GetLastError());
        }
        HWND hotkeyWnd = CreateWindowExA(
            0, kHotkeyClass, "", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, reinterpret_cast<HINSTANCE>(module), NULL);
        if (hotkeyWnd)
        {
            SetWindowLongPtrA(hotkeyWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            if (RegisterHotKey(hotkeyWnd, 1, MOD_CONTROL, VK_SPACE))
            {
                self->i_hotkeyWithShift = false;
                self->i_hotkeyRegistered = true;
            }
            else if (RegisterHotKey(hotkeyWnd, 1, MOD_CONTROL | MOD_SHIFT, VK_SPACE))
            {
                self->i_hotkeyWithShift = true;
                self->i_hotkeyRegistered = true;
            }
        }
        else
        {
            AEPinyinLog("hotkey: message-only window failed, err=%lu", GetLastError());
        }
        AEPinyinLog(
            "hotkey: %s",
            self->i_hotkeyRegistered ? (self->i_hotkeyWithShift ? "ctrl+shift+space" : "ctrl+space")
                                     : "NOT registered");

        // --- mouse side buttons --------------------------------------------
        HHOOK hook = SetWindowsHookExA(WH_MOUSE_LL, S_MouseProc, reinterpret_cast<HINSTANCE>(module), 0);
        AEPinyinLog("mouse hook: %s (this thread only)", hook ? "installed" : "FAILED");

        MSG msg;
        BOOL got = FALSE;
        while ((got = GetMessageA(&msg, NULL, 0, 0)) > 0)
        {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }

        // Hand everything back explicitly rather than trusting thread teardown.
        if (hook)
        {
            UnhookWindowsHookEx(hook);
        }
        if (hotkeyWnd)
        {
            UnregisterHotKey(hotkeyWnd, 1);
            DestroyWindow(hotkeyWnd);
        }
        AEPinyinLog("input thread: exiting");
        return 0;
    }

    void StartInputThread()
    {
        i_inputThread = CreateThread(NULL, 0, &AEPinyinSearch::S_InputThread, this, 0, &i_inputThreadId);
        if (!i_inputThread)
        {
            AEPinyinLog("input thread: CreateThread failed, err=%lu", GetLastError());
        }
    }

    bool CreatePumpWindow()
    {
        static const char* kPumpClass = "AEPinyinSearchPumpWnd";
        HMODULE module = OwnModule(reinterpret_cast<const void*>(&AEPinyinSearch::S_PumpWndProc));

        WNDCLASSA wc = {};
        wc.lpfnWndProc = S_PumpWndProc;
        wc.hInstance = reinterpret_cast<HINSTANCE>(module);
        wc.lpszClassName = kPumpClass;
        if (!RegisterClassA(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        {
            AEPinyinLog("pump: RegisterClassA failed, err=%lu", GetLastError());
            return false;
        }
        i_pumpWnd = CreateWindowExA(
            0, kPumpClass, "", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, reinterpret_cast<HINSTANCE>(module), NULL);
        if (!i_pumpWnd)
        {
            AEPinyinLog("pump: message-only window failed, err=%lu", GetLastError());
            return false;
        }
        SetWindowLongPtrA(i_pumpWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
        return true;
    }

    void ReleaseEverything()
    {
        if (i_inputThread)
        {
            PostThreadMessageA(i_inputThreadId, WM_QUIT, 0, 0);
            WaitForSingleObject(i_inputThread, 3000);
            CloseHandle(i_inputThread);
            i_inputThread = NULL;
        }
        if (i_pumpWnd)
        {
            DestroyWindow(i_pumpWnd);
            i_pumpWnd = NULL;
        }
        delete i_popup;
        i_popup = NULL;
        s_instance = NULL;
    }

    static SPAPI A_Err S_CommandHook(
        AEGP_GlobalRefcon plugin_refconP, /* >> */
        AEGP_CommandRefcon refconP,       /* >> */
        AEGP_Command command,             /* >> */
        AEGP_HookPriority hook_priority,  /* >> currently always AEGP_HP_BeforeAE */
        A_Boolean already_handledB,       /* >> */
        A_Boolean* handledPB)             /* << whether you handled */
    {
        PT_XTE_START
        {
            reinterpret_cast<AEPinyinSearch*>(refconP)->CommandHook(command, hook_priority, already_handledB, handledPB);
        }
        PT_XTE_CATCH_RETURN_ERR;
    }

    static A_Err S_UpdateMenuHook(
        AEGP_GlobalRefcon plugin_refconP, /* >> */
        AEGP_UpdateMenuRefcon refconP,    /* >> */
        AEGP_WindowType active_window)    /* >> */
    {
        PT_XTE_START
        {
            reinterpret_cast<AEPinyinSearch*>(plugin_refconP)->UpdateMenuHook(active_window);
        }
        PT_XTE_CATCH_RETURN_ERR;
    }

    // The host tells us it is going away; give the OS its hook, hotkey and
    // window back instead of leaking them for the rest of the process lifetime.
    static A_Err S_DeathHook(
        AEGP_GlobalRefcon plugin_refconP, /* >> */
        AEGP_DeathRefcon refconP)         /* >> */
    {
        // no PT_XTE wrapper: this is the last call, and throwing out of it would
        // be worse than reporting the error directly
        AEPinyinSearch* self = reinterpret_cast<AEPinyinSearch*>(plugin_refconP);
        if (!self)
        {
            return A_Err_NONE;
        }
        self->ReleaseEverything();
        AEPinyinLog("plugin: released input thread, hook, hotkey and windows on host death");
        return A_Err_NONE;
    }

    AEPinyinSearch(SPBasicSuite* pica_basicP, AEGP_PluginID pluginID)
        : i_pica_basicP(pica_basicP),
          i_pluginID(pluginID),
          i_sp(pica_basicP),
          i_popup(NULL),
          i_pumpWnd(NULL),
          i_inputThread(NULL),
          i_inputThreadId(0),
          i_hotkeyWithShift(false),
          i_hotkeyRegistered(false)
    {
        PT_ETX(i_sp.CommandSuite1()->AEGP_GetUniqueCommand(&i_command));
        PT_ETX(i_sp.CommandSuite1()->AEGP_InsertMenuCommand(
            i_command, STR(StrID_Name), AEGP_Menu_WINDOW, AEGP_MENU_INSERT_SORTED));

        PT_ETX(i_sp.RegisterSuite5()->AEGP_RegisterCommandHook(
            i_pluginID, AEGP_HP_BeforeAE, i_command, &AEPinyinSearch::S_CommandHook, (AEGP_CommandRefcon)(this)));
        PT_ETX(i_sp.RegisterSuite5()->AEGP_RegisterUpdateMenuHook(i_pluginID, &AEPinyinSearch::S_UpdateMenuHook, NULL));
        PT_ETX(i_sp.RegisterSuite5()->AEGP_RegisterDeathHook(i_pluginID, &AEPinyinSearch::S_DeathHook, (AEGP_DeathRefcon)(this)));

        i_popup = new PinyinPopup(pica_basicP, pluginID);
        // Ask the host for the names it registered: the index only carries the
        // .aex file names for third-party effects, and addProperty() needs the
        // host's own name or match name.
        i_effectNames.Build(pica_basicP);
        i_popup->SetEffectNames(&i_effectNames);

        s_instance = this;
        if (CreatePumpWindow())
        {
            StartInputThread();
        }

        AEPinyinLog("plugin: loaded (popup=%p, pump=%p, input thread=%lu, effect names=%d)", (void*)i_popup,
                    (void*)i_pumpWnd, i_inputThreadId, static_cast<int>(i_effectNames.Size()));
    }

    void CommandHook(
        AEGP_Command command,            /* >> */
        AEGP_HookPriority hook_priority, /* >> currently always AEGP_HP_BeforeAE */
        A_Boolean already_handledB,      /* >> */
        A_Boolean* handledPB)            /* << whether you handled */
    {
        if (command == i_command)
        {
            TogglePopup();
        }
    }

    void UpdateMenuHook(AEGP_WindowType active_window) /* >> */
    {
        PT_ETX(i_sp.CommandSuite1()->AEGP_EnableCommand(i_command));
        PT_ETX(i_sp.CommandSuite1()->AEGP_CheckMarkMenuCommand(
            i_command, (i_popup && i_popup->IsShown()) ? TRUE : FALSE));
    }
};

AEPinyinSearch* AEPinyinSearch::s_instance = NULL;

A_Err EntryPointFunc(
    struct SPBasicSuite* pica_basicP,  /* >> */
    A_long major_versionL,             /* >> */
    A_long minor_versionL,             /* >> */
    AEGP_PluginID aegp_plugin_id,      /* >> */
    AEGP_GlobalRefcon* global_refconP) /* << */
{
    PT_XTE_START
    {
        *global_refconP = (AEGP_GlobalRefcon) new AEPinyinSearch(pica_basicP, aegp_plugin_id);
    }
    PT_XTE_CATCH_RETURN_ERR;
}
