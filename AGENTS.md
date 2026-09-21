# ae-pinyin-search

给 After Effects 做拼音搜索。目标：打 `gsmh` 就能找到「高斯模糊」这类效果/预设，回车即应用，不用切输入法、不用打中文。

## 前提（本机事实）

- Adobe After Effects 2026（26.0），装在 `H:\adobe\Adobe After Effects 2026`。
- AE 界面语言中文，但要兼容英文界面（两份名称索引都生成）。
- Windows + PowerShell 5.1 是日常终端；Python 3.14.6 + pypinyin 0.55.0 可用；Node v24.14.1 可用。
- CEP 的 `PlayerDebugMode`（CSXS.9/10/11/12）本机已为 1，未签名扩展可直接加载，**不要改注册表**。
- 已装第三方效果：Boris FX Sapphire 2025.5、Trapcode（Mir/Tao）、Red Giant 等，效果清单必须从 AE 自身枚举，网上清单不全。

## 两条交付路线（并行）

1. **微软拼音自定义词库**（零侵入、立刻可用）：由 `app.effects` + 预设名生成 `mschxudp` 格式 `.dat`，导入后打拼音上屏中文名，再粘进 AE 原生搜索框。
   二进制格式与生成器沿用 `H:\1\generate_mspy_dict.py` 已验证的实现，不要重新发明。
2. **AE 内拼音搜索**（真正的"打拼音就出结果"）：**原生 .aex 浮窗** —— 快捷键 / 鼠标侧键唤出，弹在鼠标旁，拼音实时匹配，回车应用到选中图层，失焦或 Esc 自动关。
   界面照抄下面那份已停用的 HTML 面板配色（Fluent 深色 + 选中行 accent 竖条 + 种类徽章）。

## 禁令

- AE 安装目录**只允许增删本插件自己的文件**（`Support Files\Plug-ins\Extensions\AEPinyinSearch.aex`）；AE 自带的任何文件都不改，不碰注册表（CEP debug 已开）。
- 效果/预设的名称数据一律以 AE 自身枚举结果为准，不写死猜测的译名。
- 词库条目的拼音必须经 `pypinyin` 生成，不手工拼。

## 构建与部署（原生 .aex，当前交付形态）

- 工程在 SDK 示例树里（vcxproj 要用 `..\..\..\Headers`，所以不能搬走）：`H:\ae-sdk\AfterEffectsSDK_26.5_win\Examples\AEGP\AEPinyinSearch`。
- 构建（必须先设 `AE_PLUGIN_BUILD_DIR`，否则报 `AE_PLUGIN_BUILD_DIR is not set`）：
  `AE_PLUGIN_BUILD_DIR=H:\ae-sdk\build` + `MSBuild <Win>\AEPinyinSearch.vcxproj /p:Configuration=Release /p:Platform=x64`。
- 产物自检：`python tools\verify_aex.py`（退出码＝问题数）。**探针必须分编码**：`char*` 字面量是 UTF-8，`L"…"` 宽字面量是 UTF-16LE —— 用错编码会把好产物判成坏产物。
- 部署：`tools\rebuild_aex.ps1 -Deploy`（内部提权跑 `tools\deploy_aex.ps1`，装进 `<AE>\Support Files\Plug-ins\Extensions`）。**部署前必须关掉 AE**：.aex 被加载后文件被锁，覆盖会失败；脚本按"目标文件能否独占打开"等待，不会被没加载插件的 headless 残留进程卡住。
- 仓库里的 `aex\` 只是源码镜像（供版本控制/阅读），构建仍在 SDK 树；同步用 `powershell -File tools\sync_aex_sources.ps1`，**新增源文件要同步加进它的文件清单**。
- **装完新插件/预设后重建（一条命令）**：`powershell -NoProfile -ExecutionPolicy Bypass -File tools\rebuild_aex.ps1`（加 `-Deploy` 连部署一起做）。
  它依次跑：重扫插件目录 → 合并成 `build\ae-index.json` → 重新生成 `pinyin_data.h/.cpp`（含 `@` 分类别名）→ 匹配层自检 → 编 .aex → 探针校验 → 同步仓库镜像；**退出码＝失败步骤数**。
  索引是编译进 .aex 的，装了新效果不重建就搜不到。
- 两个自检（退出码都是问题数）：`tools\build_match_test.cmd`（匹配层在 AE 外面跑，改了 `pinyin_match.h` 必跑）、`python tools\verify_aex.py`（产物探针）。
- 查询语法是**多过滤器**（`pinyin_match.h` 的 `Query`）：`@` 选类（`@厂商` / `@名字前缀`，`@` 单独打列出所有类）、`#` 选类型（`#效果` / `#预设`，`#` 单独打列出类型）、其余当自由文本；**三者可任意顺序组合**（`@bfx #效果 blur`）。
  浏览行（`@`/`#` 出来的行）回车不是应用，而是把该过滤填进搜索框（`GroupInfo::key`）。加新维度时照这个模式：解析器收成过滤器字段 + 一个"单独打就列出来"的浏览器入口，别做成互斥的模式枚举。
- `@` 别名表由 `tools\gen_pinyin_data.py` 从索引自动生成（手动短名在它顶部的 `MANUAL_ALIASES`），**新厂商不用改 C++**。
- 浮窗交互定版：列表最多 10 行（`kMaxRows`）、靠近屏幕底部时**向上展开**（`i_growUp` + 钉住底边）、**单击即应用**（`WM_LBUTTONUP`；不能用 `LBN_SELCHANGE` —— 点已经是当前的那行不发通知）、**空查询显示最近使用**（`RecentHits`，按 `UsageTable` 的 `lastUsed` 降序取 `kMaxRows` 个，`Hit::score` 在该模式下装的是使用次数）。
  `ResetSearch()` 清空文本不会触发 `EN_CHANGE`，所以 `Show()` 里必须显式再调一次 `RunSearch()`，且要在定位之后（它会按 `i_posY`/`i_growUp` 把窗口撑开）。
- 诊断：加载/热键/钩子/应用失败都会写 `%TEMP%\AEPinyinSearch.log`（ASCII），出问题先看它；常用记录在 `%APPDATA%\AEPinyinSearch\usage.tsv`。

## 已排除的路线（别再试）

- **改写 AE 原生搜索框**：Adobe 给扩展的只有 JSX 脚本接口，没有任何 UI 控件读写能力；兜底的 Windows UI Automation 也拿不到节点（本机实测 `AE main window not found via UIA`，AE 界面自绘）。
- **进程注入 / 远程线程**：风险远大于收益，未获用户同意前不做。
- **AEGP 面板做浮窗**：`AEGP_PanelSuite1` 只有 `RegisterCreatePanelHook / UnRegister / SetTitle / ToggleVisibility / IsShown`，**没有"浮动"接口**；要浮窗只能自己建顶层窗口（现在的做法）。
- **CEP 面板 / ScriptUI 浮窗**：都试过并放弃（CEP 定位、全局热键、焦点都绕；ScriptUI 手感差），已由 .aex 取代。

## 验证方式

- 词库：生成后回读 `.dat`（条目数、拼音字段、`mschxudp` 头）自检，退出码 = 问题数。
- 面板：先在 1 个效果上跑通"输入拼音 → 命中 → 应用"全链路，再谈批量。
- 需要用户在 AE 里实测的环节，先发提问并等结果，不自行猜测。

## 项目记忆

`~/.dsh/memory/ae-pinyin-search/project_memory.md`（开工先读，收尾回写）
