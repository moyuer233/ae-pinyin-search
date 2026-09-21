/*******************************************************************/
/*                                                                 */
/* AE Pinyin Search - floating search window (Windows)             */
/*                                                                 */
/*******************************************************************/

#include "PinyinPopup.h"

#include "AEGP_SuiteHandler.h"
#include "DiagLog.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>

#ifndef ECM_FIRST
    #define ECM_FIRST 0x1500
#endif
#ifndef EM_SETCUEBANNER
    #define EM_SETCUEBANNER (ECM_FIRST + 1)
#endif
#ifndef EM_SETMARGINS
    #define EM_SETMARGINS (ECM_FIRST + 3)
#endif
#ifndef EC_LEFTMARGIN
    #define EC_LEFTMARGIN 0x0001
#endif
#ifndef EC_RIGHTMARGIN
    #define EC_RIGHTMARGIN 0x0002
#endif

namespace {

const wchar_t* kWndClass = L"AEPinyinSearchPopup";
const wchar_t* kHint = L"拼音 / 首字母 / 英文 → 回车应用并关闭；@ 只看某一类";
const wchar_t* kNoHit = L"没有匹配项";
const wchar_t* kNoGroup = L"没有这一类（试试只打 @）";

constexpr int kEditID = 2001;
constexpr int kListID = 2002;

// Logical pixels at 96 dpi; Scale() applies the window's dpi.
constexpr int kWidth = 460;
constexpr int kEditH = 32;
constexpr int kRowH = 26;
constexpr int kMaxRows = 10;

// Palette lifted from the earlier CEP/HTML panel (Fluent dark).
const COLORREF kBg = RGB(32, 32, 32);       // #202020
const COLORREF kBorder = RGB(58, 58, 58);   // #3a3a3a
const COLORREF kText = RGB(243, 243, 243);  // #f3f3f3
const COLORREF kText2 = RGB(197, 197, 197); // #c5c5c5
const COLORREF kText3 = RGB(154, 154, 154); // #9a9a9a
const COLORREF kSelBg = RGB(74, 74, 74);    // #4a4a4a
const COLORREF kAccent = RGB(76, 194, 255); // #4cc2ff
const COLORREF kAccentDim = RGB(30, 95, 138);
const COLORREF kStrokeStrong = RGB(69, 69, 69);

// Escape a string for use inside a single-quoted ExtendScript literal. Names
// come from the user's own plug-in and preset folders, so a quote or a newline
// in one of them must not be able to end the literal early.
std::string EscapeJs(const char* s)
{
    static const char* kHex = "0123456789abcdef";
    std::string out;
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(s); p && *p; ++p)
    {
        const unsigned char c = *p;
        if (c == '\\' || c == '\'' || c == '"')
        {
            out += '\\';
            out += static_cast<char>(c);
        }
        else if (c < 0x20 || c == 0x7F)
        {
            out += "\\u00";
            out += kHex[(c >> 4) & 0x0F];
            out += kHex[c & 0x0F];
        }
        else
        {
            out += static_cast<char>(c);
        }
    }
    return out;
}

std::string ToUtf8(const std::wstring& w)
{
    if (w.empty())
    {
        return std::string();
    }
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), NULL, 0, NULL, NULL);
    std::string out(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), &out[0], n, NULL, NULL);
    return out;
}

// Best effort: rounded corners need Windows 11; older Windows just ignores it.
void TryRoundCorners(HWND hWnd)
{
    typedef HRESULT(WINAPI * DwmSetWindowAttributeFn)(HWND, DWORD, LPCVOID, DWORD);
    HMODULE dwm = LoadLibraryW(L"dwmapi.dll");
    if (!dwm)
    {
        return;
    }
    DwmSetWindowAttributeFn setAttr =
        reinterpret_cast<DwmSetWindowAttributeFn>(GetProcAddress(dwm, "DwmSetWindowAttribute"));
    if (setAttr)
    {
        const DWORD kCornerPreference = 33; // DWMWA_WINDOW_CORNER_PREFERENCE
        const int kRound = 2;               // DWMWCP_ROUND
        setAttr(hWnd, kCornerPreference, &kRound, sizeof(kRound));
    }
    FreeLibrary(dwm);
}

// Our own .aex sits under <AE>\Support Files\Plug-ins\... , so walking up from
// the module path finds the Presets folder without hard-coding an install path.
std::wstring FindPresetsRoot(HINSTANCE inst)
{
    wchar_t module[MAX_PATH] = {};
    if (!GetModuleFileNameW(inst, module, MAX_PATH))
    {
        return std::wstring();
    }
    std::wstring dir(module);
    for (int depth = 0; depth < 6; ++depth)
    {
        const size_t slash = dir.find_last_of(L"\\/");
        if (slash == std::wstring::npos)
        {
            break;
        }
        dir.resize(slash);
        const std::wstring candidate = dir + L"\\Support Files\\Presets";
        const DWORD attrs = GetFileAttributesW(candidate.c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY))
        {
            return candidate;
        }
    }
    return std::wstring();
}

// Turns "AEGP_ExecuteScript" re-entry into something RAII-managed.
struct ApplyingGuard
{
    bool& flag;
    explicit ApplyingGuard(bool& f) : flag(f) { flag = true; }
    ~ApplyingGuard() { flag = false; }
};

} // namespace

PinyinPopup::PinyinPopup(SPBasicSuite* spbP, AEGP_PluginID pluginID)
    : i_spbP(spbP),
      i_pluginID(pluginID),
      i_hWnd(NULL),
      i_editH(NULL),
      i_listH(NULL),
      i_inst(NULL),
      i_prevEditProc(NULL),
      i_prevListProc(NULL),
      i_prevForeground(NULL),
      i_fontName(NULL),
      i_fontSmall(NULL),
      i_bgBrush(NULL),
      i_editBrush(NULL),
      i_selBrush(NULL),
      i_accentBrush(NULL),
      i_borderPen(NULL),
      i_badgePen(NULL),
      i_badgePenSel(NULL),
      i_dpi(0),
      i_width(0),
      i_posX(0),
      i_posY(0),
      i_growUp(false),
      i_shown(false),
      i_activated(false),
      i_applying(false),
      i_suppressChange(false),
      i_groupMode(false)
{
}

PinyinPopup::~PinyinPopup()
{
    if (i_hWnd)
    {
        DestroyWindow(i_hWnd);
        i_hWnd = NULL;
    }
    if (i_fontName)
    {
        DeleteObject(i_fontName);
    }
    if (i_fontSmall)
    {
        DeleteObject(i_fontSmall);
    }
    if (i_bgBrush)
    {
        DeleteObject(i_bgBrush);
    }
    if (i_editBrush)
    {
        DeleteObject(i_editBrush);
    }
    if (i_selBrush)
    {
        DeleteObject(i_selBrush);
    }
    if (i_accentBrush)
    {
        DeleteObject(i_accentBrush);
    }
    if (i_borderPen)
    {
        DeleteObject(i_borderPen);
    }
    if (i_badgePen)
    {
        DeleteObject(i_badgePen);
    }
    if (i_badgePenSel)
    {
        DeleteObject(i_badgePenSel);
    }
}

int PinyinPopup::Scale(int logical) const
{
    const int dpi = (i_dpi > 0) ? i_dpi : 96;
    return MulDiv(logical, dpi, 96);
}

std::wstring PinyinPopup::Utf8ToWide(const char* utf8)
{
    if (!utf8 || !*utf8)
    {
        return std::wstring();
    }
    // With -1 the returned count INCLUDES the terminating NUL, so the buffer has
    // to hold n characters - sizing it n-1 made this write one wchar_t past the
    // end of the string.
    const int n = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    if (n <= 0)
    {
        return std::wstring();
    }
    std::wstring out(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, &out[0], n);
    out.resize(static_cast<size_t>(n) - 1);
    return out;
}

std::wstring PinyinPopup::PresetsRoot() const
{
    return FindPresetsRoot(i_inst);
}

// Where the usage history lives. Keyed by entry name, so an index rebuild (a new
// plug-in was installed) does not lose or mis-attribute it.
std::wstring PinyinPopup::UsagePath() const
{
    wchar_t base[MAX_PATH] = {};
    if (GetEnvironmentVariableW(L"APPDATA", base, MAX_PATH) == 0)
    {
        return std::wstring();
    }
    const std::wstring dir = std::wstring(base) + L"\\AEPinyinSearch";
    CreateDirectoryW(dir.c_str(), NULL); // already there is fine
    return dir + L"\\usage.tsv";
}

void PinyinPopup::LoadUsage()
{
    i_usage.Clear();
    const std::string path = ToUtf8(UsagePath());
    if (path.empty())
    {
        return;
    }
    FILE* f = NULL;
    if (fopen_s(&f, path.c_str(), "r") != 0 || !f)
    {
        return; // first run
    }
    char line[1024];
    while (std::fgets(line, sizeof(line), f))
    {
        char* tab = std::strchr(line, '\t');
        if (!tab)
        {
            continue;
        }
        *tab = '\0';
        const int count = std::atoi(line);
        char* name = tab + 1;
        size_t len = std::strlen(name);
        while (len > 0 && (name[len - 1] == '\n' || name[len - 1] == '\r'))
        {
            name[--len] = '\0';
        }
        i_usage.Add(name, count);
    }
    std::fclose(f);
    AEPinyinLog("usage: loaded %d name(s) from %s", static_cast<int>(i_usage.Size()), path.c_str());
}

void PinyinPopup::SaveUsage()
{
    const std::string path = ToUtf8(UsagePath());
    if (path.empty())
    {
        return;
    }
    FILE* f = NULL;
    if (fopen_s(&f, path.c_str(), "w") != 0 || !f)
    {
        AEPinyinLog("usage: cannot write %s", path.c_str());
        return;
    }
    for (size_t i = 0; i < i_usage.Size(); ++i)
    {
        std::fprintf(f, "%d\t%s\n", i_usage.CountAt(i), i_usage.NameAt(i).c_str());
    }
    std::fclose(f);
}

void PinyinPopup::EnsureDpi()
{
    if (!i_hWnd)
    {
        return;
    }

    int dpi = 0;
    typedef UINT(WINAPI * GetDpiForWindowFn)(HWND);
    static GetDpiForWindowFn getDpiForWindow = reinterpret_cast<GetDpiForWindowFn>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"));
    if (getDpiForWindow)
    {
        dpi = static_cast<int>(getDpiForWindow(i_hWnd));
    }
    if (dpi <= 0)
    {
        HDC dc = GetDC(i_hWnd);
        dpi = GetDeviceCaps(dc, LOGPIXELSX);
        ReleaseDC(i_hWnd, dc);
    }
    if (dpi <= 0)
    {
        dpi = 96;
    }
    if (dpi == i_dpi && i_fontName)
    {
        return;
    }

    i_dpi = dpi;
    i_width = Scale(kWidth);

    if (i_fontName)
    {
        DeleteObject(i_fontName);
    }
    if (i_fontSmall)
    {
        DeleteObject(i_fontSmall);
    }
    i_fontName = CreateFontW(
        -Scale(13), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_TT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
    i_fontSmall = CreateFontW(
        -Scale(12), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_TT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");

    if (i_editH && i_fontName)
    {
        SendMessageW(i_editH, WM_SETFONT, reinterpret_cast<WPARAM>(i_fontName), TRUE);
        SendMessageW(i_editH, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(Scale(12), Scale(12)));
    }
}

bool PinyinPopup::Create()
{
    if (i_hWnd)
    {
        return true;
    }

    HMODULE module = NULL;
    GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&PinyinPopup::S_WndProc), &module);
    i_inst = reinterpret_cast<HINSTANCE>(module);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = PinyinPopup::S_WndProc;
    wc.hInstance = i_inst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = kWndClass;
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    {
        AEPinyinLog("popup: RegisterClassExW failed, err=%lu", GetLastError());
        return false;
    }

    i_hWnd = CreateWindowExW(
        WS_EX_TOOLWINDOW, kWndClass, L"拼音搜索", WS_POPUP | WS_CLIPCHILDREN, 0, 0, Scale(kWidth),
        Scale(kEditH) + 2, NULL, NULL, i_inst, this);
    if (!i_hWnd)
    {
        AEPinyinLog("popup: CreateWindowExW failed, err=%lu", GetLastError());
        return false;
    }

    TryRoundCorners(i_hWnd);

    i_bgBrush = CreateSolidBrush(kBg);
    i_editBrush = CreateSolidBrush(kBg);
    i_selBrush = CreateSolidBrush(kSelBg);
    i_accentBrush = CreateSolidBrush(kAccent);
    i_borderPen = CreatePen(PS_SOLID, 1, kBorder);
    i_badgePen = CreatePen(PS_SOLID, 1, kStrokeStrong);
    i_badgePenSel = CreatePen(PS_SOLID, 1, kAccentDim);

    i_editH = CreateWindowExW(
        0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0, 10, 10, i_hWnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kEditID)), i_inst, NULL);
    i_listH = CreateWindowExW(
        0, L"LISTBOX", L"",
        WS_CHILD | WS_VSCROLL | LBS_NOTIFY | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS | LBS_NOINTEGRALHEIGHT,
        0, 0, 10, 10, i_hWnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kListID)), i_inst, NULL);

    if (!i_editH || !i_listH || !i_bgBrush || !i_editBrush)
    {
        AEPinyinLog(
            "popup: control/GDI creation failed edit=%p list=%p (err=%lu)", i_editH, i_listH,
            GetLastError());
        DestroyWindow(i_hWnd); // do not leave a half-built window behind: Create()
        i_hWnd = NULL;         // would short-circuit on it and hand Show() a wreck
        return false;
    }

    EnsureDpi();

    SendMessageW(i_editH, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(kHint));
    SendMessageW(i_editH, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(Scale(12), Scale(12)));

    i_prevEditProc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(i_editH, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&PinyinPopup::S_EditProc)));
    i_prevListProc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(i_listH, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&PinyinPopup::S_ListProc)));
    SetWindowLongPtrW(i_editH, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    SetWindowLongPtrW(i_listH, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    Layout();
    return true;
}

void PinyinPopup::Layout()
{
    if (!i_hWnd)
    {
        return;
    }
    RECT rc;
    GetClientRect(i_hWnd, &rc);
    const int w = rc.right - rc.left;
    const int h = rc.bottom - rc.top;
    const int bar = Scale(kEditH) + 1; // 1px top border

    if (i_editH)
    {
        MoveWindow(i_editH, 1, 1, w - 2, Scale(kEditH), TRUE);
    }
    if (i_listH)
    {
        MoveWindow(i_listH, 1, bar, w - 2, h - bar - 1, TRUE);
    }
}

void PinyinPopup::ResizeToRows(int rows)
{
    if (!i_hWnd)
    {
        return;
    }
    rows = (std::max)(0, (std::min)(rows, kMaxRows));
    const int barH = Scale(kEditH) + 2;
    const int h = barH + rows * Scale(kRowH);
    // Growing upwards keeps the bottom edge where it was, so a bar summoned near
    // the bottom of the screen grows up into empty space instead of off-screen.
    const int y = i_growUp ? (i_posY + barH - h) : i_posY;
    SetWindowPos(i_hWnd, NULL, i_posX, y, i_width, h, SWP_NOZORDER | SWP_NOACTIVATE);
    Layout();
    InvalidateRect(i_hWnd, NULL, TRUE);
}

std::wstring PinyinPopup::Query() const
{
    if (!i_editH)
    {
        return std::wstring();
    }
    const int len = GetWindowTextLengthW(i_editH);
    if (len <= 0)
    {
        return std::wstring();
    }
    std::wstring s(static_cast<size_t>(len) + 1, L'\0');
    GetWindowTextW(i_editH, &s[0], len + 1);
    s.resize(static_cast<size_t>(len));
    return s;
}

void PinyinPopup::ResetSearch()
{
    if (i_editH)
    {
        // Setting the text fires EN_CHANGE, which would run the search a second
        // time (and against the text we are about to replace anyway).
        i_suppressChange = true;
        SetWindowTextW(i_editH, L"");
        i_suppressChange = false;
    }
    if (i_listH)
    {
        SendMessageW(i_listH, LB_RESETCONTENT, 0, 0);
    }
    i_hits.clear();
    i_groups.clear();
    i_groupMode = false;
    ResizeToRows(0);
}

void PinyinPopup::RunSearch()
{
    if (!i_listH || !i_editH)
    {
        return;
    }

    const std::string q = ToUtf8(Query());
    const pinyin::Query parsed = pinyin::ParseQuery(q);

    i_groupMode = (parsed.kind == pinyin::QueryKind::GroupList);
    if (i_groupMode)
    {
        // "@" on its own: list the classes, biggest first.
        pinyin::ListGroups(i_groups, 2);
        i_hits.clear();
    }
    else
    {
        i_groups.clear();
        pinyin::SearchQuery(q, i_hits, 200, &i_usage);
    }

    SendMessageW(i_listH, LB_RESETCONTENT, 0, 0);

    if (i_groupMode)
    {
        for (const pinyin::GroupInfo& g : i_groups)
        {
            SendMessageW(i_listH, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Utf8ToWide(g.name.c_str()).c_str()));
        }
    }
    else
    {
        for (const pinyin::Hit& h : i_hits)
        {
            const PinyinEntry& e = kPinyinEntries[h.index];
            SendMessageW(i_listH, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Utf8ToWide(e.name).c_str()));
        }
    }

    const size_t rows = i_groupMode ? i_groups.size() : i_hits.size();
    if (rows == 0)
    {
        SendMessageW(i_listH, LB_SETCURSEL, static_cast<WPARAM>(-1), 0);
        ShowWindow(i_listH, SW_HIDE);
        const std::wstring query = Query();
        ResizeToRows(query.empty() ? 0 : 1); // one row holds the "no match" line
    }
    else
    {
        SendMessageW(i_listH, LB_SETCURSEL, 0, 0);
        ShowWindow(i_listH, SW_SHOW);
        ResizeToRows(static_cast<int>(rows));
    }
}

void PinyinPopup::MoveSelection(int delta)
{
    if (!i_listH)
    {
        return;
    }
    const int count = static_cast<int>(SendMessageW(i_listH, LB_GETCOUNT, 0, 0));
    if (count <= 0)
    {
        return;
    }
    int sel = static_cast<int>(SendMessageW(i_listH, LB_GETCURSEL, 0, 0));
    if (sel < 0)
    {
        sel = 0;
    }
    sel += delta;
    if (sel < 0)
    {
        sel = 0;
    }
    if (sel >= count)
    {
        sel = count - 1;
    }
    SendMessageW(i_listH, LB_SETCURSEL, sel, 0);
    SendMessageW(i_listH, LB_SETTOPINDEX, sel, 0);
    InvalidateRect(i_listH, NULL, TRUE);
}

void PinyinPopup::Show()
{
    if (!Create())
    {
        return;
    }

    LoadUsage();
    ResetSearch();

    POINT pt = {};
    GetCursorPos(&pt);
    MONITORINFO mi = {};
    mi.cbSize = sizeof(mi);
    HMONITOR monitor = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    GetMonitorInfoW(monitor, &mi);

    int x = pt.x - Scale(64);
    const int w = i_width;
    const int barH = Scale(kEditH) + 2;
    const int maxH = barH + kMaxRows * Scale(kRowH);
    const int workTop = static_cast<int>(mi.rcWork.top);
    const int workBottom = static_cast<int>(mi.rcWork.bottom);
    const int workRight = static_cast<int>(mi.rcWork.right);
    const int workLeft = static_cast<int>(mi.rcWork.left);

    // Decide the direction from the FULL grown window, not from the bar: flipping
    // only after the results have already run off the bottom is what made a bar
    // summoned near the bottom show almost nothing.
    i_growUp = (pt.y + Scale(20) + maxH > workBottom);
    int y = i_growUp ? (pt.y - Scale(8) - barH) : (pt.y + Scale(20));
    if (i_growUp && y < workTop)
    {
        i_growUp = false; // not enough room above either; fall back to growing down
        y = pt.y + Scale(20);
    }

    x = (std::max)(workLeft, (std::min)(x, workRight - w));
    if (i_growUp)
    {
        y = (std::max)(workTop, (std::min)(y, workBottom - barH));
    }
    else
    {
        // Keep the whole grown list on screen when there is room; otherwise the
        // bar itself must stay visible and the list scrolls.
        y = (std::max)(workTop, (std::min)(y, workBottom - barH));
    }
    i_posX = x;
    i_posY = y;

    i_prevForeground = GetForegroundWindow();
    SetWindowPos(i_hWnd, HWND_TOP, x, y, w, barH, SWP_SHOWWINDOW | SWP_NOACTIVATE);
    i_shown = true;
    SetForegroundWindow(i_hWnd);
    EnsureDpi(); // the popup may have landed on a monitor with another scaling
    SetFocus(i_editH);
}

void PinyinPopup::Hide()
{
    if (!i_hWnd || !i_shown)
    {
        return;
    }
    i_shown = false;
    i_activated = false;
    ShowWindow(i_hWnd, SW_HIDE);
    if (i_prevForeground && IsWindow(i_prevForeground))
    {
        SetForegroundWindow(i_prevForeground);
    }
    i_prevForeground = NULL;
}

void PinyinPopup::Toggle()
{
    if (i_shown)
    {
        Hide();
    }
    else
    {
        Show();
    }
}

void PinyinPopup::ApplySelected()
{
    if (!i_listH || i_applying)
    {
        return; // AEGP_ExecuteScript pumps messages; do not re-enter
    }
    const int sel = static_cast<int>(SendMessageW(i_listH, LB_GETCURSEL, 0, 0));
    if (i_groupMode)
    {
        if (sel < 0 || sel >= static_cast<int>(i_groups.size()))
        {
            return;
        }
        // Turning a class row into "@class" is what makes Enter useful on the
        // list you get from typing a bare "@". Short vendors keep their alias
        // ("Boris FX" -> "@BFX"), the rest fold to a single token.
        const std::string key = pinyin::AliasForVendor(i_groups[sel].name);
        if (key.empty())
        {
            return;
        }
        const std::wstring text = L"@" + Utf8ToWide(key.c_str());
        SetWindowTextW(i_editH, text.c_str());
        SendMessageW(i_editH, EM_SETSEL, text.size(), text.size());
        return;
    }
    if (sel < 0 || sel >= static_cast<int>(i_hits.size()))
    {
        return;
    }
    ApplyEntry(i_hits[sel].index);
}

void PinyinPopup::ApplyEntry(int entryIndex)
{
    if (entryIndex < 0 || entryIndex >= kPinyinEntryCount)
    {
        return;
    }
    const PinyinEntry& e = kPinyinEntries[entryIndex];

    ApplyingGuard guard(i_applying);

    // The layer bookkeeping lives in ExtendScript, so the apply step is a tiny
    // script. It answers with ASCII status codes only, so nothing here has to
    // deal with the encoding of the reply.
    std::string jsx = "(function(){";
    jsx += "var c=app.project.activeItem;";
    jsx += "if(!c||!(c instanceof CompItem))return 'NOCOMP';";
    jsx += "var L=c.selectedLayers;";
    jsx += "if(!L||!L.length)return 'NOLAYER';";

    if (e.is_preset && e.path && *e.path)
    {
        jsx += "var roots=[";
        const std::string root = ToUtf8(PresetsRoot());
        if (!root.empty())
        {
            jsx += "'";
            jsx += EscapeJs(root.c_str());
            jsx += "',";
        }
        jsx += "];";
        jsx += "var f=null;";
        jsx += "for(var r=0;r<roots.length;r++){";
        jsx += "var t=new File(roots[r]+'/";
        jsx += EscapeJs(e.path);
        jsx += "');if(t.exists){f=t;break;}}";
        jsx += "if(!f)return 'NOPRESET';";
        jsx += "var it=null;try{it=app.project.importFile(new ImportOptions(f));}catch(x){return 'NOPRESET';}";
        jsx += "var n=0;for(var i=0;i<L.length;i++){try{L[i].applyPreset(it);n++;}catch(x){}}";
        jsx += "return n?'OK':'NOPRESET';";
    }
    else
    {
        jsx += "var names=['";
        jsx += EscapeJs(e.name ? e.name : "");
        jsx += "'";
        if (e.english && *e.english && std::strcmp(e.english, e.name) != 0)
        {
            jsx += ",'";
            jsx += EscapeJs(e.english);
            jsx += "'";
        }
        jsx += "];var n=0;";
        jsx += "for(var i=0;i<L.length;i++){for(var j=0;j<names.length;j++){";
        jsx += "try{L[i].property('ADBE Effect Parade').addProperty(names[j]);n++;break;}catch(x){}}}";
        jsx += "return n?'OK':'NOEFFECT';";
    }
    jsx += "})();";

    AEGP_SuiteHandler suites(i_spbP);
    AEGP_MemHandle resultH = NULL;
    AEGP_MemHandle errH = NULL;
    const A_Err err =
        suites.UtilitySuite6()->AEGP_ExecuteScript(i_pluginID, jsx.c_str(), FALSE, &resultH, &errH);

    std::string status;
    if (err == A_Err_NONE && resultH)
    {
        void* p = NULL;
        if (suites.MemorySuite1()->AEGP_LockMemHandle(resultH, &p) == A_Err_NONE && p)
        {
            status.assign(static_cast<const char*>(p));
            suites.MemorySuite1()->AEGP_UnlockMemHandle(resultH);
        }
    }
    if (resultH)
    {
        suites.MemorySuite1()->AEGP_FreeMemHandle(resultH);
    }
    if (errH)
    {
        suites.MemorySuite1()->AEGP_FreeMemHandle(errH);
    }

    if (status.compare(0, 2, "OK") == 0)
    {
        // Remember the choice: next time this one ranks above its peers.
        i_usage.Bump(e.name);
        SaveUsage();
        ResetSearch();
        Hide();
        return;
    }

    AEPinyinLog("apply: status='%s' err=%d entry='%s'", status.c_str(), static_cast<int>(err),
                e.name ? e.name : "");

    // Failures are the only thing worth interrupting for.
    const wchar_t* why = L"应用失败";
    if (status == "NOCOMP")
    {
        why = L"当前没有打开的合成";
    }
    else if (status == "NOLAYER")
    {
        why = L"先在时间线里选中至少一个图层";
    }
    else if (status == "NOEFFECT")
    {
        why = L"AE 里找不到这个效果";
    }
    else if (status == "NOPRESET")
    {
        why = L"预设文件缺失或应用失败";
    }
    else if (err != A_Err_NONE)
    {
        why = L"ExtendScript 执行失败";
    }

    std::wstring text = why;
    if (e.name && *e.name)
    {
        text += L"：";
        text += Utf8ToWide(e.name);
    }
    // Own the box by the window the popup took the foreground from, so it does
    // not end up behind the host.
    HWND owner = i_prevForeground;
    Hide();
    MessageBoxW(owner, text.c_str(), L"拼音搜索", MB_OK | MB_ICONWARNING);
}

void PinyinPopup::DrawRow(const DRAWITEMSTRUCT& dis)
{
    RECT rc = dis.rcItem;
    const bool sel = (dis.itemState & ODS_SELECTED) != 0;

    // Always start from a clean row: an out-of-range itemID used to leave the
    // previous pixels behind.
    FillRect(dis.hDC, &rc, sel ? i_selBrush : i_bgBrush);
    SetBkMode(dis.hDC, TRANSPARENT);

    std::wstring name;
    std::wstring secondary;
    const wchar_t* badge = L"";
    if (i_groupMode)
    {
        if (dis.itemID >= i_groups.size())
        {
            return;
        }
        const pinyin::GroupInfo& g = i_groups[dis.itemID];
        name = Utf8ToWide(g.name.c_str());
        secondary = std::to_wstring(g.count) + L" 项";
        badge = L"分类";
    }
    else
    {
        if (dis.itemID >= i_hits.size())
        {
            return;
        }
        const PinyinEntry& e = kPinyinEntries[i_hits[dis.itemID].index];
        name = Utf8ToWide(e.name);
        secondary = Utf8ToWide(e.english);
        badge = e.is_preset ? L"预设" : L"效果";
    }

    if (sel)
    {
        const int mid = (rc.top + rc.bottom) / 2;
        const int half = Scale(8);
        RECT accent = {rc.left + Scale(4), mid - half, rc.left + Scale(4) + Scale(3), mid + half};
        HGDIOBJ oldBrush = SelectObject(dis.hDC, i_accentBrush);
        HGDIOBJ oldPen = SelectObject(dis.hDC, GetStockObject(NULL_PEN));
        RoundRect(dis.hDC, accent.left, accent.top, accent.right, accent.bottom, Scale(4), Scale(4));
        SelectObject(dis.hDC, oldPen);
        SelectObject(dis.hDC, oldBrush);
    }

    // Right-hand kind badge.
    SelectObject(dis.hDC, i_fontSmall);
    SIZE badgeText = {};
    GetTextExtentPoint32W(dis.hDC, badge, static_cast<int>(std::wcslen(badge)), &badgeText);
    const int badgeW = badgeText.cx + Scale(12);
    const int badgeH = Scale(16);
    RECT badgeBox = {rc.right - Scale(8) - badgeW, (rc.top + rc.bottom - badgeH) / 2,
                     rc.right - Scale(8), (rc.top + rc.bottom + badgeH) / 2};
    HGDIOBJ oldPen = SelectObject(dis.hDC, sel ? i_badgePenSel : i_badgePen);
    HGDIOBJ oldBrush = SelectObject(dis.hDC, GetStockObject(HOLLOW_BRUSH));
    RoundRect(dis.hDC, badgeBox.left, badgeBox.top, badgeBox.right, badgeBox.bottom, Scale(4), Scale(4));
    SelectObject(dis.hDC, oldBrush);
    SelectObject(dis.hDC, oldPen);
    SetTextColor(dis.hDC, sel ? kAccent : kText3);
    DrawTextW(dis.hDC, badge, -1, &badgeBox, DT_SINGLELINE | DT_VCENTER | DT_CENTER | DT_NOPREFIX);

    // Name, then the secondary text in a dimmer, smaller face.
    RECT text = rc;
    text.left += Scale(12);
    text.right = badgeBox.left - Scale(8);

    SelectObject(dis.hDC, i_fontName);
    SetTextColor(dis.hDC, sel ? kText : kText2);
    SIZE nameSize = {};
    GetTextExtentPoint32W(dis.hDC, name.c_str(), static_cast<int>(name.size()), &nameSize);

    if (!secondary.empty() && std::wcscmp(name.c_str(), secondary.c_str()) != 0 &&
        text.left + nameSize.cx + Scale(8) < text.right)
    {
        RECT nameRect = text;
        nameRect.right = text.left + nameSize.cx;
        DrawTextW(dis.hDC, name.c_str(), -1, &nameRect, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

        RECT secondRect = text;
        secondRect.left = nameRect.right + Scale(8);
        SelectObject(dis.hDC, i_fontSmall);
        SetTextColor(dis.hDC, kText3);
        DrawTextW(dis.hDC, secondary.c_str(), -1, &secondRect,
                  DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    }
    else
    {
        DrawTextW(dis.hDC, name.c_str(), -1, &text,
                  DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    }
}

LRESULT CALLBACK PinyinPopup::S_WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    PinyinPopup* self = reinterpret_cast<PinyinPopup*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    if (!self && message == WM_NCCREATE)
    {
        CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<PinyinPopup*>(cs->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    if (self)
    {
        return self->WndProc(hWnd, message, wParam, lParam);
    }
    return DefWindowProcW(hWnd, message, wParam, lParam);
}

LRESULT PinyinPopup::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_ERASEBKGND:
        return 1; // fully painted in WM_PAINT

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc;
        GetClientRect(hWnd, &rc);

        FillRect(hdc, &rc, i_bgBrush);

        HGDIOBJ oldPen = SelectObject(hdc, i_borderPen);
        HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
        Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);

        const bool empty = i_groupMode ? i_groups.empty() : i_hits.empty();
        if (empty && !Query().empty())
        {
            RECT text = {rc.left + Scale(12), Scale(kEditH) + 1, rc.right - Scale(12), rc.bottom};
            SelectObject(hdc, i_fontSmall);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, kText3);
            const pinyin::Query parsed = pinyin::ParseQuery(ToUtf8(Query()));
            const wchar_t* line = (parsed.kind == pinyin::QueryKind::GroupFilter) ? kNoGroup : kNoHit;
            DrawTextW(hdc, line, -1, &text, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
        }

        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_SIZE:
        Layout(); // ResizeToRows() already called it; this keeps manual resizes right
        return 0;

    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC:
        SetTextColor(reinterpret_cast<HDC>(wParam), kText);
        SetBkColor(reinterpret_cast<HDC>(wParam), kBg);
        return reinterpret_cast<LRESULT>(i_editBrush);

    case WM_MEASUREITEM:
    {
        MEASUREITEMSTRUCT* mis = reinterpret_cast<MEASUREITEMSTRUCT*>(lParam);
        if (mis->CtlID == kListID)
        {
            mis->itemHeight = Scale(kRowH);
            return TRUE;
        }
        break;
    }

    case WM_DRAWITEM:
    {
        DRAWITEMSTRUCT* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
        if (dis->CtlID == kListID)
        {
            DrawRow(*dis);
            return TRUE;
        }
        break;
    }

    case WM_COMMAND:
        if (LOWORD(wParam) == kEditID && HIWORD(wParam) == EN_CHANGE)
        {
            if (!i_suppressChange)
            {
                RunSearch();
            }
            return 0;
        }
        if (LOWORD(wParam) == kListID && HIWORD(wParam) == LBN_DBLCLK)
        {
            ApplySelected();
            return 0;
        }
        break;

    case WM_ACTIVATE:
        if (LOWORD(wParam) == WA_INACTIVE)
        {
            if (i_shown && i_activated)
            {
                Hide(); // click anywhere else and it goes away, like an IME candidate bar
            }
        }
        else
        {
            i_activated = true;
        }
        return 0;

    case WM_CLOSE:
        Hide();
        return 0;

    default:
        break;
    }

    return DefWindowProcW(hWnd, message, wParam, lParam);
}

LRESULT CALLBACK PinyinPopup::S_EditProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    PinyinPopup* self = reinterpret_cast<PinyinPopup*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    if (self)
    {
        return self->EditProc(hWnd, message, wParam, lParam);
    }
    return DefWindowProcW(hWnd, message, wParam, lParam);
}

LRESULT PinyinPopup::EditProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_KEYDOWN:
        switch (wParam)
        {
        case VK_RETURN:
            ApplySelected();
            return 0;
        case VK_ESCAPE:
            Hide();
            return 0;
        case VK_DOWN:
            MoveSelection(1);
            return 0;
        case VK_UP:
            MoveSelection(-1);
            return 0;
        case VK_NEXT:
            MoveSelection(kMaxRows);
            return 0;
        case VK_PRIOR:
            MoveSelection(-kMaxRows);
            return 0;
        default:
            break;
        }
        break;

    case WM_CHAR:
        if (wParam == VK_RETURN)
        {
            return 0; // Enter is handled in WM_KEYDOWN; swallow the beep
        }
        break;

    default:
        break;
    }

    if (i_prevEditProc)
    {
        return CallWindowProcW(i_prevEditProc, hWnd, message, wParam, lParam);
    }
    return DefWindowProcW(hWnd, message, wParam, lParam);
}

LRESULT CALLBACK PinyinPopup::S_ListProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    PinyinPopup* self = reinterpret_cast<PinyinPopup*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    if (self)
    {
        return self->ListProc(hWnd, message, wParam, lParam);
    }
    return DefWindowProcW(hWnd, message, wParam, lParam);
}

LRESULT PinyinPopup::ListProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_MOUSEMOVE:
    {
        // Hovering a row makes it the current one, so Enter applies what the
        // pointer is pointing at.
        const LRESULT pos = SendMessageW(hWnd, LB_ITEMFROMPOINT, 0, lParam);
        if (HIWORD(pos) == 0)
        {
            const int idx = LOWORD(pos);
            if (idx != static_cast<int>(SendMessageW(hWnd, LB_GETCURSEL, 0, 0)))
            {
                SendMessageW(hWnd, LB_SETCURSEL, idx, 0);
                InvalidateRect(hWnd, NULL, TRUE);
            }
        }
        break;
    }

    case WM_LBUTTONUP:
    {
        // A single click applies. This cannot be done with LBN_SELCHANGE:
        // clicking the row that is already current sends no notification, and
        // the first row is current as soon as a search returns.
        const LRESULT pos = SendMessageW(hWnd, LB_ITEMFROMPOINT, 0, lParam);
        if (HIWORD(pos) == 0)
        {
            SendMessageW(hWnd, LB_SETCURSEL, LOWORD(pos), 0);
            ApplySelected();
            return 0;
        }
        break;
    }

    default:
        break;
    }

    if (i_prevListProc)
    {
        return CallWindowProcW(i_prevListProc, hWnd, message, wParam, lParam);
    }
    return DefWindowProcW(hWnd, message, wParam, lParam);
}
