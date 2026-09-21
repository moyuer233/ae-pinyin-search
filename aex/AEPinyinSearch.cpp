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

class AEPinyinSearch
{
public:
    SPBasicSuite* i_pica_basicP;
    AEGP_PluginID i_pluginID;

    AEGP_SuiteHandler i_sp;
    AEGP_Command i_command;
    PinyinPopup* i_popup;
    HWND i_hotkeyWnd;
    HHOOK i_mouseHook;
    bool i_hotkeyWithShift;
    bool i_hotkeyRegistered;

    /// STATIC BINDERS
    static LRESULT CALLBACK S_HotkeyWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        AEPinyinSearch* self = reinterpret_cast<AEPinyinSearch*>(GetWindowLongPtrA(hWnd, GWLP_USERDATA));
        if (self)
        {
            return self->HotkeyWndProc(hWnd, message, wParam, lParam);
        }
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    LRESULT HotkeyWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        if (message == WM_HOTKEY && wParam == 1)
        {
            // A global hotkey also fires while some other app is in front; the
            // search bar belongs to AE, so ignore those.
            if (IsAEForeground())
            {
                TogglePopup();
            }
            return 0;
        }
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    // Register a global hotkey so the search bar can be summoned without first
    // going through the host's Keyboard Shortcuts dialog. Ctrl+Space is the
    // requested binding; it is also the Windows IME switch, so if the OS refuses
    // it we fall back to Ctrl+Shift+Space.
    void RegisterHotkey()
    {
        static const char* kClassName = "AEPinyinSearchHotkeyWnd";

        // Use the DLL's own instance, not the host's: the window class and the
        // window must belong to the same module as this code.
        HMODULE module = NULL;
        GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&AEPinyinSearch::S_HotkeyWndProc), &module);

        WNDCLASSA wc = {};
        wc.lpfnWndProc = S_HotkeyWndProc;
        wc.hInstance = reinterpret_cast<HINSTANCE>(module);
        wc.lpszClassName = kClassName;
        if (!RegisterClassA(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        {
            AEPinyinLog("hotkey: RegisterClassA failed, err=%lu", GetLastError());
        }

        i_hotkeyWnd = CreateWindowExA(
            0, kClassName, "", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, reinterpret_cast<HINSTANCE>(module), NULL);
        if (!i_hotkeyWnd)
        {
            AEPinyinLog("hotkey: message-only window failed, err=%lu", GetLastError());
            return;
        }
        SetWindowLongPtrA(i_hotkeyWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

        if (RegisterHotKey(i_hotkeyWnd, 1, MOD_CONTROL, VK_SPACE))
        {
            i_hotkeyWithShift = false;
            i_hotkeyRegistered = true;
        }
        else if (RegisterHotKey(i_hotkeyWnd, 1, MOD_CONTROL | MOD_SHIFT, VK_SPACE))
        {
            i_hotkeyWithShift = true;
            i_hotkeyRegistered = true;
        }
        AEPinyinLog(
            "hotkey: %s",
            i_hotkeyRegistered ? (i_hotkeyWithShift ? "ctrl+shift+space" : "ctrl+space") : "NOT registered");
    }

    static AEPinyinSearch* s_instance;

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

    void TogglePopup()
    {
        if (i_popup)
        {
            i_popup->Toggle();
        }
    }

    // Mouse side buttons: a low-level hook lets XBUTTON1/2 summon the search bar
    // without any vendor driver remapping. The callback only inspects the
    // message, so it stays cheap.
    static LRESULT CALLBACK S_MouseProc(int nCode, WPARAM wParam, LPARAM lParam)
    {
        if (nCode >= 0 && wParam == WM_XBUTTONDOWN && s_instance && IsAEForeground())
        {
            const MSLLHOOKSTRUCT* ms = reinterpret_cast<const MSLLHOOKSTRUCT*>(lParam);
            const int which = static_cast<int>((ms->mouseData >> 16) & 0xFFFF);
            if (which == XBUTTON1 || which == XBUTTON2)
            {
                s_instance->TogglePopup();
            }
        }
        return CallNextHookEx(NULL, nCode, wParam, lParam);
    }

    void RegisterMouseHook()
    {
        s_instance = this;
        HMODULE module = NULL;
        GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&AEPinyinSearch::S_MouseProc), &module);
        i_mouseHook = SetWindowsHookExA(WH_MOUSE_LL, S_MouseProc, reinterpret_cast<HINSTANCE>(module), 0);
        if (!i_mouseHook)
        {
            AEPinyinLog("mouse hook: SetWindowsHookExA failed, err=%lu", GetLastError());
        }
    }

    void ReleaseEverything()
    {
        if (i_mouseHook)
        {
            UnhookWindowsHookEx(i_mouseHook);
            i_mouseHook = NULL;
        }
        if (i_hotkeyWnd)
        {
            UnregisterHotKey(i_hotkeyWnd, 1);
            DestroyWindow(i_hotkeyWnd);
            i_hotkeyWnd = NULL;
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
        AEPinyinLog("plugin: released hook/hotkey/window on host death");
        return A_Err_NONE;
    }

    AEPinyinSearch(SPBasicSuite* pica_basicP, AEGP_PluginID pluginID)
        : i_pica_basicP(pica_basicP),
          i_pluginID(pluginID),
          i_sp(pica_basicP),
          i_popup(NULL),
          i_hotkeyWnd(NULL),
          i_mouseHook(NULL),
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

        RegisterHotkey();
        RegisterMouseHook();

        AEPinyinLog("plugin: loaded (popup=%p, mouse hook=%p)", (void*)i_popup, (void*)i_mouseHook);
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
