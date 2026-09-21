/*******************************************************************/
/*                                                                 */
/* AE Pinyin Search - floating search window (Windows)             */
/*                                                                 */
/* The AEGP panel suite has no "float this panel" call, so the      */
/* search bar is a plain top-level popup window that this plug-in   */
/* owns end to end. It mirrors the look of the earlier CEP/HTML     */
/* panel: dark Fluent surface, an accent bar on the selected row,   */
/* and a kind badge on the right of every row.                      */
/*                                                                 */
/*******************************************************************/

#ifndef PINYIN_POPUP_H
#define PINYIN_POPUP_H

#include "AE_GeneralPlug.h"

#include <windows.h>

#include <string>
#include <vector>

#include "../pinyin_match.h"

class PinyinPopup
{
public:
    PinyinPopup(SPBasicSuite* spbP, AEGP_PluginID pluginID);
    ~PinyinPopup();

    // Show it if hidden, hide it if shown.
    void Toggle();

    void Show();
    void Hide();

    bool IsShown() const { return i_shown; }

private:
    PinyinPopup(const PinyinPopup&);
    PinyinPopup& operator=(const PinyinPopup&);

    bool Create();
    void Layout();
    void ResizeToRows(int rows);
    void EnsureDpi();
    void RunSearch();
    void ApplySelected();
    void ApplyEntry(int entryIndex);
    void MoveSelection(int delta);
    void DrawRow(const DRAWITEMSTRUCT& dis);
    void ResetSearch();
    std::wstring Query() const;
    int Scale(int logical) const;
    std::wstring PresetsRoot() const;
    std::wstring UsagePath() const;
    void LoadUsage();
    void SaveUsage();
    static std::wstring Utf8ToWide(const char* utf8);

    static LRESULT CALLBACK S_WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    static LRESULT CALLBACK S_EditProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT EditProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    static LRESULT CALLBACK S_ListProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT ListProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    SPBasicSuite* i_spbP;
    AEGP_PluginID i_pluginID;

    HWND i_hWnd;
    HWND i_editH;
    HWND i_listH;
    HINSTANCE i_inst;
    WNDPROC i_prevEditProc;
    WNDPROC i_prevListProc;
    HWND i_prevForeground;

    HFONT i_fontName;
    HFONT i_fontSmall;
    HBRUSH i_bgBrush;
    HBRUSH i_editBrush;
    HBRUSH i_selBrush;
    HBRUSH i_accentBrush;
    HPEN i_borderPen;
    HPEN i_badgePen;
    HPEN i_badgePenSel;

    int i_dpi;
    int i_width;
    int i_posX; // top-left chosen when the popup was shown; the list grows from
    int i_posY; // there, upwards when i_growUp is set
    bool i_growUp;
    bool i_shown;
    bool i_activated;
    bool i_applying;        // guards AEGP_ExecuteScript re-entry
    bool i_suppressChange;  // ResetSearch must not trigger a second search
    bool i_groupMode;       // the rows are classes, not entries
    bool i_kindMode;        // the rows are kinds (effect / preset)

    pinyin::UsageTable i_usage; // "%APPDATA%\AEPinyinSearch\usage.tsv"

    std::vector<pinyin::Hit> i_hits;
    std::vector<pinyin::GroupInfo> i_groups;
};

#endif // PINYIN_POPUP_H
