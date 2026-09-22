# AE 拼音搜索

给 After Effects 2026 用的拼音搜索浮窗。按热键或鼠标侧键唤出，在鼠标旁边弹出条状输入框。
如：打 `gsmh` 就出「高斯模糊」，回车（或单击）应用到当前选中图层，告别翻效果栏，频繁切换输入法。

当前版本 v0.6.0 · [下载 AEPinyinSearch.aex](https://github.com/moyuer233/ae-pinyin-search/releases)

![license](https://img.shields.io/badge/license-MIT-blue) ![release](https://img.shields.io/github/v/release/moyuer233/ae-pinyin-search) ![downloads](https://img.shields.io/github/downloads/moyuer233/ae-pinyin-search/total)

## 特性

- 唤出：`Ctrl+Space` 全局热键（被输入法占用时自动退到 `Ctrl+Shift+Space`）、鼠标侧键 X1/X2、
  或 `窗口 > 拼音搜索` 菜单项（可在 AE 的 `编辑 > 键盘快捷键` 里自己绑）。
- 位置，鼠标旁，靠近屏幕底部时自动向上展开，列表最多 10 行；向上展开时列表在输入框上方、输入框停在唤出的位置不动。点别处自动关，`Esc`，`Ctrl+Space` 直接关，应用成功后自动清空并关窗。
- 匹配：全拼 `gaosimohu`、首字母 `gsmh`、中英混合 `gaussian`、模糊音（zh→z / ch→c / sh→s / ang→an / eng→en / ing→in）。
- `@` 分类（像MC的JEI模组那样）：
  - `@` 单独打 → 列出所有类（Boris FX-481个 / Sapphire-287个 / Red Giant Universe-105个 …），回车把该类填成 `@bfx`
  - `@BCC` → 只看名字以 BCC 开头的那批；`@BFX` = `@Boris FX`，`@RGU` = `@Red Giant Universe`
- `#` 类型：`#效果` / `#预设`（也认 `#effect` / `#preset`）；`#` 单独打列出类型，回车把它填进搜索框。
- 两个过滤可以同时用，顺序随意，后面还能跟关键词：`@bfx #效果 blur` = Boris FX 的效果里再筛 blur。
  每个过滤各只认第一个：`@bfx @rgu` 只按 `@bfx` 过滤（第二个 `@` 会被忽略）。
- 唤出就是"最近用过的"：还没打字时直接列出最近用过的 10 个（从最近到最远，行尾标「用过 N 次」），回车或单击即可再用。
- 常用快捷搜索：应用成功的条目会记一笔（`%APPDATA%\AEPinyinSearch\usage.tsv`），同级里用得多的先出现。
- 日志：加载、热键注册、钩子安装、应用失败自动导出到 `%TEMP%\AEPinyinSearch.log`。

## 安装

1. 从 [Releases](https://github.com/moyuer233/ae-pinyin-search/releases) 下载 `AEPinyinSearch.aex`。
2. 放进 `<AE 安装目录>\Support Files\Plug-ins\Extensions\`
   放这里不需要管理员权限；**别同时放两份**（两个实例会抢热键和鼠标钩子）。
3. 重启 AE。

## 使用

| 键 | 作用 |
|---|---|
| `Ctrl+Space` / 鼠标侧键 | 显示 / 隐藏 |
| 打字 | 实时搜索（拼音 / 首字母 / 英文 / 中文） |
| 什么都不打 | 最近用过的 10 个（最近的排最前） |
| `@…` | 只看某一类（见上） |
| `#…` | 只看某一类型（效果 / 预设） |
| `@… #… 关键词` | 三类条件叠加，顺序随意 |
| `↑` `↓` / 鼠标悬停 | 选行 |
| `回车` / 单击 | 应用到当前选中图层（预设走 `applyPreset`） |
| `Esc` | 关窗 |


## 从源码构建

需要 Adobe 的 After Effects SDK（不随本仓库分发）与 MSVC。工程必须放在 SDK 示例树里，因为 vcxproj 用 `..\..\..\Headers` 找头文件：

```
<AE SDK>\Examples\AEGP\AEPinyinSearch\      ← 本仓库 aex\ 里的源码镜像放这里
```

一条命令跑完整条链（重扫插件目录 → 合索引 → 生成 C++ 数据与 `@` 别名 → 匹配层自检 → 编译 → 产物探针 → 部署）：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\rebuild_aex.ps1 -Deploy
```

退出码＝失败步骤数。**装了新效果插件后必须重跑**：索引是编译进 `.aex` 的，不重建就搜不到新插件。

换机器 / 换 AE 版本时用环境变量覆盖路径（不设就用脚本里的默认值）：
`AE_INSTALL_DIR`、`AE_SDK_DIR`、`AE_PLUGIN_BUILD_DIR`、`AE_MEDIACORE`、`MSBUILD_EXE`、`DUMPBIN_EXE`、`VCVARS64`。

两个自检可以单独跑：`tools\build_match_test.cmd`（匹配层在 AE 外面跑，退出码＝失败数）、`python tools\verify_aex.py`（产物字符串/PE 探针，另外比对产物是否比源码新、版本串是否与源码一致）。

索引不齐就会让这一步失败（不是静默跳过）：某个输入目录找不到、条目数低于下限、非效果目录混进来，都会让重建链非零退出。

## 索引机制

- AE 自带中文词典 `Support Files\Dictionaries\zh_CN\after_effects_zh_CN.dat`（效果名）
- AE 预设目录 `Support Files\Presets\**\*.ffx`（预设名与相对路径）
- 两个插件目录里的 `.aex` 文件名 + 所在厂商目录（第三方效果；本机第三方 1274 条 + 内置 223 条 = 效果 1497 条）
- `Plug-ins` 下的 `Format` / `Keyframe` / `Extensions` 三个目录不算效果（导入导出器、关键帧助手、扩展管理器，AE 的效果菜单里根本没有它们），扫描时跳过

## 说明与限制

- 目前只有 Windows 实现（UI 在 `Win\` 下，匹配层与索引是平台无关的）。
- 第三方效果名取的是插件目录里的 `.aex` 文件名，中文界面下 AE 显示的名字可能与它略有差异；预设应用依赖预设目录结构。
- 产物里没有版本字段可看，加载时会把版本写进 `%TEMP%\AEPinyinSearch.log`（`plugin: loaded v…`），想知道装的是哪一版看那一行。
