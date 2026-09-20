# Crunch EQ — 交互清单（Interactions）

> 只读分析产出。逐条从代码核实，标注 `文件:行号`（路径相对 `Crunch EQ Source/`）。
> 分三组：**A. 仓库代码中明确实现的交互**；**B. JUCE 内建行为（仓库用标准控件、未禁用也未覆盖）**；
> **C. 经 grep 全仓库确认不存在的交互**。最后是文档声明与代码的差异。

## A. 仓库代码实现的交互

### A1. 旋钮（BandControlPanel 的 6 个 Slider）

| 交互 | 行为 | 代码出处 |
|------|------|----------|
| 拖动方向 | `RotaryVerticalDrag`：垂直拖拽，上=增大、下=减小 | `src/ui/BandControlPanel.cpp:75` |
| 旋转角度 | 1.25π–2.75π（270°） | `src/ui/BandControlPanel.cpp:77-78` |
| 数值显示 | `TextBoxBelow`、56×16、**只读**（`setTextBoxStyle(..., false, ...)` 第二参 false）、后缀 Hz/dB/% | `src/ui/BandControlPanel.cpp:76`、`src/ui/BandControlPanel.cpp:79` |
| 参数绑定 | 6 个 SliderAttachment（freq/gain/q/in/out/colorAmount） | `src/ui/BandControlPanel.cpp:54-55`、`src/ui/BandControlPanel.cpp:63` |
| 频段旋钮随选中频段重绑 | `setSelectedBand()` 里 reset 后重建 attachment | `src/ui/BandControlPanel.cpp:90-95`、`src/ui/BandControlPanel.cpp:117-119` |
| 无选中频段时 | freq/gain/q/type/slope/bypass 全部 `setEnabled(false)` | `src/ui/BandControlPanel.cpp:97-106` |

### A2. DisplayView 频谱图上的鼠标交互

| 交互 | 行为 | 代码出处 |
|------|------|----------|
| 单击频点 | 选中该频段；经回调联动底部面板重绑 | `src/ui/DisplayView.cpp:213-224`；回调接线 `src/PluginEditor.cpp:29` |
| 单击空白 | 取消选中（`setSelectedBand(-1)`） | `src/ui/DisplayView.cpp:225-233` |
| 拖拽频点 | 水平→频率（对数映射 xToFreq）；垂直→增益（yToDb，按 gainScale） | `src/ui/DisplayView.cpp:249-255` |
| Ctrl / Command + 拖拽 | 仅调 Q：按拖拽点的 x 位置映射 Q（xToQ） | `src/ui/DisplayView.cpp:218`、`src/ui/DisplayView.cpp:244-248` |
| 拖拽阈值 | 位移 < 3px 内不生效（避免误触） | `src/ui/DisplayView.cpp:241-242` |
| 拖拽结束 | 清空拖拽状态（mouseUp） | `src/ui/DisplayView.cpp:258-263` |
| **右键频点** | 删除该频段（Enabled=false），并选中下一个可用频段 | `src/ui/DisplayView.cpp:205-211`；`removeBand` 实现 `src/ui/DisplayView.cpp:310-329` |
| 右键空白 | 无任何行为（直接 return，无右键菜单） | `src/ui/DisplayView.cpp:205-211` |
| **滚轮在频点上** | 调 Q：向上 ×1.1、向下 ÷1.1，夹在 0.025–40 | `src/ui/DisplayView.cpp:341-350` |
| 滚轮在空白 | 无行为 | `src/ui/DisplayView.cpp:343-345` |
| **双击空白** | 新增频点：左缘(t<0.08)→LowCut，右缘(t>0.92)→HighCut，其余→Bell 且增益取自点击高度；Q=1.0；自动占用第一个未启用频段并选中 | `src/ui/DisplayView.cpp:331-339`；`addBandAt` 实现 `src/ui/DisplayView.cpp:265-308` |
| 双击频点 | **无任何行为**（hitTest 命中时只走 `if (band < 0)` 分支之外，直接结束） | `src/ui/DisplayView.cpp:331-339` |
| 命中测试 | 遍历 24 频段，命中半径 18px，取最近者 | `src/ui/DisplayView.cpp:177-201` |

### A3. 齿轮按钮与主题菜单

| 交互 | 行为 | 代码出处 |
|------|------|----------|
| 齿轮按钮（⚙, 24×24, 左上角） | Tooltip "Settings"；点击弹出主题菜单 | `src/ui/DisplayView.cpp:10-12`、`src/ui/DisplayView.cpp:22` |
| 菜单项 | "Theme: Blue" / "Theme: Red" / "Theme: White" 三项，选中后写入 processor 主题原子 + LAF setTheme + 触发 `onThemeChanged` 全局换肤 | `src/ui/DisplayView.cpp:156-175`；回调 `src/PluginEditor.cpp:30` |

### A4. 其他控件交互

| 交互 | 行为 | 代码出处 |
|------|------|----------|
| Bypass 按钮 | 点击切换（`setClickingTogglesState(true)`），绑定到 `band{i}Enabled`（每频段旁路） | `src/ui/BandControlPanel.cpp:44-46`、`src/ui/BandControlPanel.cpp:124` |
| 8 个 ComboBox | Type/Slope/Phase/Quality/GainScale/Analyzer/ColorMode/ColorPosition，ComboBoxAttachment 绑定 | `src/ui/BandControlPanel.cpp:19-42`、`src/ui/BandControlPanel.cpp:57-64` |
| 参数变化联动重绘 | 编辑器注册全部参数的 APVTS listener，任何参数变化即 `display_`/`panel_` repaint | `src/PluginEditor.cpp:23-27`、`src/PluginEditor.cpp:58-62` |
| 实时刷新 | DisplayView 60 Hz `juce::Timer`：拉频谱 + 算响应曲线 + 电平表衰减 + repaint | `src/ui/DisplayView.cpp:7`、`src/ui/DisplayView.cpp:129-142` |
| 电平表 | 峰值保持：只升不降；每 tick 降 1 dB（≈60 dB/s），下限 −60 dB | `src/ui/DisplayView.cpp:133-139`；绘制 `src/ui/DisplayView.cpp:555-597` |

## B. JUCE 内建行为（仓库未覆盖，也未禁用）

> 以下行为来自标准 `juce::Slider` / `juce::Component` 的默认实现，仓库代码中**没有对应实现行**，
> 仅列出"默认值生效的上下文行"作为佐证。

| 交互 | 行为 | 依据 |
|------|------|------|
| 旋钮双击复位 | `juce::Slider` 默认 `setDoubleClickReturnValue(true)`：双击旋钮/滑块回到参数默认值。仓库 `setupKnob` 未调用 `setDoubleClickReturnValue(false)`，`BandControlPanel` 也没有重写 `mouseDoubleClick` | 旋钮创建处 `src/ui/BandControlPanel.cpp:71-84`；全仓库唯一 `mouseDoubleClick` 在 DisplayView（`src/ui/DisplayView.cpp:331`），与旋钮无关 |
| Shift 微调 | 标准 `juce::Slider` 拖拽时按住 Shift 减速（fine adjust）。仅对 BandControlPanel 的 6 个原生 Slider 生效 | `src/ui/BandControlPanel.cpp:75` 使用原生 Slider；`DisplayView::mouseDrag` 完全不检查 Shift（只读 Ctrl/Command）`src/ui/DisplayView.cpp:236-256` |
| 旋钮滚轮调值 | 原生 Slider 默认滚轮步进。DisplayView 的 `mouseWheelMove` 覆盖只作用于自身频点命中场景，不影响面板旋钮 | `src/ui/BandControlPanel.cpp:74`（未覆盖 mouseWheelMove）；`src/ui/DisplayView.cpp:341-350` |
| Tooltip | 原生 Component 默认悬停显示 tooltip 机制，但全仓库只有一处设置了文本 | `src/ui/DisplayView.cpp:11` |

## C. 经全仓库 grep 确认不存在的交互

| 交互 | 结论 | 核实方式 |
|------|------|----------|
| A/B 状态对比 | **不存在**（无 A/B 按钮、无状态快照） | grep `A/B` 无结果；UI 控件清单见 `src/ui/BandControlPanel.h:21-29` |
| 预设上一/下一切换 | **不存在**（无预设系统，`getNumPrograms()==1`） | `src/PluginProcessor.h:31-35`；grep `preset` 无结果 |
| 撤销 / 重做（Undo/Redo） | **不存在**：APVTS 构造时 UndoManager 传的是 `nullptr` | `src/PluginProcessor.cpp:8`；grep `undo\|Undo` 无结果 |
| 键盘快捷键 | **不存在**：无任何 `keyPressed` 重写 | grep `keyPressed\|KeyPress` 无结果 |
| 自定义右键上下文菜单 | **不存在**：右键仅用于删除频点，无菜单弹出 | `src/ui/DisplayView.cpp:205-211` |

## D. 文档声明与代码的差异（以代码为准）

| 文档 | 声明 | 代码实际 |
|------|------|----------|
| README.md:11、DESIGN.md:168-169 | "双击删除"频点 | 双击只新增（空白处）/无行为（频点上）；删除仅右键。`src/ui/DisplayView.cpp:331-339` vs `src/ui/DisplayView.cpp:205-211` |
| DESIGN.md:170 | "空白处拖拽 = 框选（预留）" | 空白处拖拽无行为。`src/ui/DisplayView.cpp:238-239` |
| DESIGN.md:176 | 刷新 ~30–60 Hz | 固定 60 Hz。`src/ui/DisplayView.cpp:7` |
