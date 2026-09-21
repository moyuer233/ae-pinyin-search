# AE 拼音搜索（After Effects 原生插件）

给 After Effects 2026 用的拼音搜索浮窗。按快捷键或鼠标侧键唤出，在鼠标旁边弹出一个小条，打 `gsmh` 就出「高斯模糊」，
回车（或单击）应用到当前选中图层 —— 不用切输入法、不用打中文、不用在几百个效果里翻。

**版本** v0.1.0 ｜ 平台 Windows ｜ AE 2026（26.x）｜ 产物 `AEPinyinSearch.aex`

## 特性

- **唤出**：`Ctrl+Space` 全局热键（被输入法占用时自动退到 `Ctrl+Shift+Space`）、鼠标侧键 X1/X2、
  或 `窗口 > 拼音搜索` 菜单项（可在 AE 的 `编辑 > 键盘快捷键` 里自己绑）。
- **弹在鼠标旁**，靠近屏幕底部时向上展开；点 AE 别处自动关，`Esc` 直接关，应用成功后自动清空并关窗。
- **匹配**：全拼 `gaosimohu`、首字母 `gsmh`、中英混合 `gaussian`、模糊音（zh→z / ch→c / sh→s / ang→an / eng→en / ing→in）。
- **`@` 分类**（像 Minecraft JEI 那样只看某一类）：
  - `@` 单独打 → 列出所有类（Boris FX 481 / Sapphire 287 / Red Giant Universe 105 …），回车把该类填成 `@bfx`
  - `@BCC` → 只看名字以 BCC 开头的那批；`@BFX` = `@Boris FX`，`@RGU` = `@Red Giant Universe`
  - 裸写厂商名（`boris`）等价于 `@boris`，并且不会藏掉其它匹配
  - `@BCC blur` → 在类内再筛
- **常用的排前面**：应用成功的条目会记一笔（`%APPDATA%\AEPinyinSearch\usage.tsv`，按名字记，重建索引也不丢），
  同级里用得多的先出现；加权上限刻意小于相关度档位差，不会让弱匹配压过精确匹配。
- **诊断**：加载、热键注册、钩子安装、应用失败都会写 `%TEMP%\AEPinyinSearch.log`。

## 装

1. 从 [Releases](../../releases) 下载 `AEPinyinSearch.aex`。
2. 放进 `<AE 安装目录>\Support Files\Plug-ins\Extensions\`（本机示例：`H:\adobe\Adobe After Effects 2026\Support Files\Plug-ins\Extensions\`）。
   放这里不需要管理员权限；**别同时放两份**（两个实例会抢热键和鼠标钩子）。
3. 重启 AE。

## 用

| 键 | 作用 |
|---|---|
| `Ctrl+Space` / 鼠标侧键 | 显示 / 隐藏 |
| 打字 | 实时搜索（拼音 / 首字母 / 英文 / 中文） |
| `@…` | 只看某一类（见上） |
| `↑` `↓` / 鼠标悬停 | 选行 |
| `回车` / 单击 | 应用到当前选中图层（预设走 `applyPreset`） |
| `Esc` | 关窗 |

没选图层时会弹一条中文提示，不会静默失败。

## 从源码构建

需要 Adobe 的 **After Effects SDK**（不随本仓库分发）与 MSVC。工程必须放在 SDK 示例树里，因为 vcxproj 用 `..\..\..\Headers` 找头文件：

```
<AE SDK>\Examples\AEGP\AEPinyinSearch\      ← 本仓库 aex\ 里的源码镜像放这里
```

一条命令跑完整条链（重扫插件目录 → 合索引 → 生成 C++ 数据与 `@` 别名 → 匹配层自检 → 编译 → 产物探针 → 部署）：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\rebuild_aex.ps1 -Deploy
```

退出码＝失败步骤数。**装了新效果插件后必须重跑**：索引是编译进 `.aex` 的，不重建就搜不到新插件。

两个自检可以单独跑：`tools\build_match_test.cmd`（匹配层在 AE 外面跑，退出码＝失败数）、`python tools\verify_aex.py`（产物字符串/PE 探针）。

## 索引怎么来

- AE 自带中文词典 `Support Files\Dictionaries\zh_CN\after_effects_zh_CN.dat`（效果名）
- AE 预设目录 `Support Files\Presets\**\*.ffx`（预设名与相对路径）
- 两个插件目录里的 `.aex` 文件名 + 所在厂商目录（第三方效果，本机 1534 条）

拼音在构建期用 `pypinyin` 算好写进 C++ 表，运行时不依赖任何东西。

## 说明与限制

- 源码基于 Adobe AE SDK 的 **Panelator** 示例改造；Adobe 的版权声明保留在文件头。本仓库不含 AE SDK 本身。
- 浮窗是插件自己创建的顶层窗口：AEGP 的面板套件（`AEGP_PanelSuite1`）没有"浮动"接口，做浮窗只能这么来。
- 目前只有 Windows 实现（UI 在 `Win\` 下，匹配层与索引是平台无关的）。
- 第三方效果名取的是插件目录里的 `.aex` 文件名，中文界面下 AE 显示的名字可能与它略有差异；预设应用依赖预设目录结构。

## 许可与来源

- 本项目自己写的文件（`aex/Win/`、`aex/pinyin_match.h`、`aex/pinyin_data.*`、`tools/`）：**MIT**，见 `LICENSE`。
- `aex/AEPinyinSearch.cpp`、`aex/AEPinyinSearch.h`、`aex/AEPinyinSearch_Strings.cpp`、`aex/PT_Err.h` 保留 Adobe 版权头，
  它们改写自 Adobe After Effects SDK 的 **Panelator** 示例。这些文件头里的 NOTICE 原文是
  "Adobe permits you to use, modify, and distribute this file in accordance with the terms of the Adobe license
  agreement accompanying it"，即按随附的 **AE SDK 许可协议**分发。
- **AE SDK 本体（Headers / Util / 其余示例）不在本仓库**，构建时由 vcxproj 用相对路径指向你自己解压的 SDK 目录；
  SDK 需要自己从 Adobe 获取。
- 本项目与 Adobe 无隶属关系，也未获得 Adobe 的背书。

