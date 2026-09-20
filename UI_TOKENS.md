# Crunch EQ — UI 令牌清单（UI Tokens）

> 只读分析产出。**重要前提：本仓库没有"设计令牌变量"体系**——所有取值都是硬编码在
> 源码里的 `juce::Colour(0xAARRGGBB)` 字面量与魔法数字。本清单是对字面量的整理，
> 出处均为 `Crunch EQ Source/` 下的文件:行号。
>
> 颜色分三个来源：① `LookAndFeel.h`（走 JUCE colour ID，作用于 Slider/ComboBox/Button/Label/PopupMenu）；
> ② `DisplayView.cpp`（自绘网格/频谱/曲线/频点/电平表，直读字面量）；
> ③ `PluginEditor.cpp`（编辑器背景，重复了一份暗色字面量）。

## 1. 主题色（accent）

三套主题由 `theme` 索引 0/1/2 选择，菜单标签为 Blue/Red/White（`src/ui/DisplayView.cpp:159-161`）。

| 令牌 | 取值 | 出处 |
|------|------|------|
| `accent`（theme 0, Blue） | `#2F9DFF` (`0xff2f9dff`) | `src/ui/LookAndFeel.h:19`；`src/PluginProcessor.cpp:242` |
| `accent`（theme 1, Red） | `#E05B5B` (`0xffe05b5b`) | `src/ui/LookAndFeel.h:17`；`src/PluginProcessor.cpp:240` |
| `accent`（theme 2, 亮色/灰） | `#4A5058` (`0xff4a5058`) | `src/ui/LookAndFeel.h:18`；`src/PluginProcessor.cpp:241` |
| `accent.selected`（选中频点填充） | `accent.brighter(0.35)` | `src/ui/DisplayView.cpp:548` |

注意：`accentForTheme()`（`src/ui/LookAndFeel.h:13-21`）与 `getThemeAccent()`（`src/PluginProcessor.cpp:236-244`）是**两份重复的同一 switch**。

## 2. 面板/控件色（LookAndFeel，暗色 / 亮色两套）

| 令牌 | 暗色 (theme 0/1) | 亮色 (theme 2) | 出处 |
|------|------------------|----------------|------|
| `bg`（窗口/编辑器背景） | `#101214` (`0xff101214`) | `#F2F3F5` (`0xfff2f3f5`) | `src/ui/LookAndFeel.h:28`；`src/PluginEditor.cpp:46` |
| `panel`（Slider/ComboBox/Button/PopupMenu 底） | `#1A1D20` (`0xff1a1d20`) | `#E8EAED` (`0xffe8eaed`) | `src/ui/LookAndFeel.h:29` |
| `track` / `outline`（旋钮轨道、ComboBox 描边） | `#2A2E33` (`0xff2a2e33`) | `#C6CAD0` (`0xffc6cad0`) | `src/ui/LookAndFeel.h:30-31` |
| `text`（控件文字） | `#D8D8D8` (`0xffd8d8d8) | `#3C4148` (`0xff3c4148`) | `src/ui/LookAndFeel.h:32` |
| `textDim`（Label 文字） | `#9AA0A6` (`0xff9aa0a6`) | `#5A6068` (`0xff5a6068`) | `src/ui/LookAndFeel.h:33` |
| `slider.textBox.bg/outline` | 全透明 `0x00000000`（不画文本框底/框） | 同左 | `src/ui/LookAndFeel.h:42-43` |
| `popup.highlight.text` | 白色 `0xffffffff` | 同左 | `src/ui/LookAndFeel.h:56` |

映射到 JUCE colour ID 的完整清单（`src/ui/LookAndFeel.h:35-56`）：
`ResizableWindow::backgroundColourId→bg`；`Slider::{background→panel, thumb→accent, track→track, rotaryFill→accent, rotaryOutline→track, textBoxText→text, textBoxBg/Outline→透明}`；
`ComboBox::{background→panel, text→text, outline→outline, arrow→text, button→panel}`；
`TextButton::{button→panel, textOff→text, textOn→accent}`；`Label::text→textDim`；
`PopupMenu::{background→panel, text→text, highlightBg→accent, highlightText→白}`。

## 3. DisplayView 自绘色（不经 LookAndFeel，直读字面量）

| 令牌 | 暗色 | 亮色 | 出处 |
|------|------|------|------|
| `display.bg` | `#14171A` (`0xff14171a`) ⚠ 与 LAF 的 `#101214` **不一致** | `#F2F3F5` | `src/ui/DisplayView.cpp:38` |
| `grid`（网格线） | `#22262B` (`0xff22262b) | `#D8DBE0` (`0xffd8dbe0`) | `src/ui/DisplayView.cpp:39` |
| `grid.zero`（0 dB 线，加粗区分） | `#3C4249` (`0xff3c4249) | `#A9AEB4` (`0xffa9aeb4`) | `src/ui/DisplayView.cpp:377` |
| `display.text`（坐标文字） | `#8A9096` (`0xff8a9096) | `#5A6068` | `src/ui/DisplayView.cpp:40` |
| `spectrum.pre.fill` | accent 渐变：顶部 α0.65 → 底部 α0.10 | 同左 | `src/ui/DisplayView.cpp:450` |
| `spectrum.post.stroke` | `#FFFFFF` | `#3C4148` | `src/ui/DisplayView.cpp:454` |
| `curve.stroke` | accent | 同左 | `src/ui/DisplayView.cpp:511-512` |
| `node.fill` | accent（未选）/ accent.brighter(0.35)（选中） | 同左 | `src/ui/DisplayView.cpp:548-549` |
| `node.ring`（选中外环） | `#FFFFFF` | `#2B2F35` | `src/ui/DisplayView.cpp:544-545` |
| `node.innerDot`（频点内点 r=3） | `#101214` | `#FFFFFF` | `src/ui/DisplayView.cpp:550-551` |
| `node.cutLine`（Cut 类频段的竖直参考线） | accent α0.40 | 同左 | `src/ui/DisplayView.cpp:535-536` |
| `meter.bg` | `#16181B` (`0xff16181b) | `#E8EAED` | `src/ui/DisplayView.cpp:573` |
| `meter.tick` | grid 色，每 20 dB 一格（−60…0） | 同左 | `src/ui/DisplayView.cpp:576-578` |
| `meter.fill` | 前景色 α0.35 + 2px 峰值帽线 | 同左 | `src/ui/DisplayView.cpp:581-584` |
| `meter.IN` | 绿 `#3AC36B` (`0xff3ac36b`) | 同左 | `src/ui/DisplayView.cpp:595` |
| `meter.OUT` | accent | 同左 | `src/ui/DisplayView.cpp:596` |

## 4. 字体

- **无自定义字体族**：全仓库无 `setTypefaceFor` 调用，全部使用 JUCE 默认字体。
- 字号令牌（仅 4 处 `setFont`）：

| 用途 | 字号 | 出处 |
|------|------|------|
| 网格频率标签（10/20…20k） | 12 | `src/ui/DisplayView.cpp:380` |
| dB 刻度标签 | 11 | `src/ui/DisplayView.cpp:404` |
| 电平表 IN/OUT 标签 | 10 | `src/ui/DisplayView.cpp:586` |
| 电平表 dB 读数 | 9 | `src/ui/DisplayView.cpp:590` |
| 旋钮数值文本框 | 56×16，`TextBoxBelow`，只读（第二个参数 false） | `src/ui/BandControlPanel.cpp:76` |
| 旋钮标签 Label | 高 16，居中 | `src/ui/BandControlPanel.cpp:137`、`src/ui/BandControlPanel.cpp:83` |
| ComboBox / COLOUR 标签 | 左对齐 `centredLeft` | `src/ui/BandControlPanel.cpp:16`、`src/ui/BandControlPanel.cpp:50` |

## 5. 圆角

**无圆角令牌**：代码中不存在圆角常量或圆角绘制。所有形状均为直角/纯几何：
- 频点 = `fillEllipse`（正圆，`src/ui/DisplayView.cpp:549`）
- 电平表 = `fillRect`（`src/ui/DisplayView.cpp:574`、`582-584`）
- 网格 = `drawVerticalLine/drawHorizontalLine`（`src/ui/DisplayView.cpp:364`、`374-378`）
- 旋钮 = 圆弧轨道（`addCentredArc` + `strokePath`，`src/ui/LookAndFeel.h:75-88`）
- ComboBox/Button/Label 走 JUCE 默认绘制（本 LookAndFeel 未覆盖其背景形状）。

## 6. 间距与控件尺寸

### 编辑器与分区

| 令牌 | 值 | 出处 |
|------|----|------|
| `editor.default` | 920×620 | `src/PluginEditor.cpp:17` |
| `editor.min` / `editor.max` | 230×155 / 1840×1240 | `src/PluginEditor.cpp:19` |
| `panel.height`（底部功能区） | 240 px（固定） | `src/PluginEditor.cpp:54` |
| `panel.padding` | 整体内缩 8 | `src/ui/BandControlPanel.cpp:129` |
| `panel.rowGap` | 行间 6 px | `src/ui/BandControlPanel.cpp:144`、`src/ui/BandControlPanel.cpp:160` |
| `control.margin` | 每个控件 `reduced(2)` | `src/ui/BandControlPanel.cpp:141-164` 各处 |
| `panel.rows` | 3 行等高（高 /3） | `src/ui/BandControlPanel.cpp:130-133` |

### 控件固定宽（BandControlPanel）

| 控件 | 宽 (px) | 出处 |
|------|---------|------|
| Type 下拉 | 90 | `src/ui/BandControlPanel.cpp:141` |
| Slope 下拉 | 90 | `src/ui/BandControlPanel.cpp:142` |
| Bypass 按钮 | 64 | `src/ui/BandControlPanel.cpp:143` |
| COLOUR 标签 | 64 | `src/ui/BandControlPanel.cpp:151` |
| Color Mode 下拉 | 88 | `src/ui/BandControlPanel.cpp:152` |
| Color Position 下拉 | 64 | `src/ui/BandControlPanel.cpp:153` |
| Amount 旋钮列 | 90 | `src/ui/BandControlPanel.cpp:154` |
| Phase Mode 下拉 | 100 | `src/ui/BandControlPanel.cpp:156` |
| Quality 下拉 | 80 | `src/ui/BandControlPanel.cpp:157` |
| Gain Scale 下拉 | 80 | `src/ui/BandControlPanel.cpp:158` |
| Analyzer 下拉 | 80 | `src/ui/BandControlPanel.cpp:159` |
| Freq/Gain/Q 旋钮列 | 行剩余宽 /3（弹性） | `src/ui/BandControlPanel.cpp:146-149` |
| In/Out 旋钮列 | 行剩余宽 /2（弹性） | `src/ui/BandControlPanel.cpp:162-164` |

### 其他尺寸

| 令牌 | 值 | 出处 |
|------|----|------|
| `settingsButton`（齿轮 ⚙） | 24×24，位于 (4,4) | `src/ui/DisplayView.cpp:10`、`src/ui/DisplayView.cpp:22` |
| `plot.meterWidth`（右侧电平表区） | 60 px | `src/ui/DisplayView.h:93` |
| `plot.padTop` / `plot.padBottom` | 16 / 24 px | `src/ui/DisplayView.h:94-95` |
| `node.radius`（频点） | 7（未选）/ 9（选中）；选中外环 +2、描边 1.5 | `src/ui/DisplayView.cpp:540`、`src/ui/DisplayView.cpp:545` |
| `node.innerDot.radius` | 3（直径 6） | `src/ui/DisplayView.cpp:551` |
| `node.hitRadius`（命中测试） | 18 px | `src/ui/DisplayView.cpp:179` |
| `dragThreshold`（拖拽生效最小位移） | 3 px | `src/ui/DisplayView.cpp:241` |
| 频率标签 | 宽 34；刻度右侧 +3（10k/20k 改左侧 −37） | `src/ui/DisplayView.cpp:390-393` |
| dB 刻度标签框 | 52×16，x=4，y−8 | `src/ui/DisplayView.cpp:411` |
| 电平表条 | 2 根，`barW = (areaW−6)/2` | `src/ui/DisplayView.cpp:558-561` |

## 7. 旋钮几何（drawRotarySlider 覆盖）

| 令牌 | 值 | 出处 |
|------|----|------|
| `knob.radius` | `min(w,h)/2 − 4` | `src/ui/LookAndFeel.h:66` |
| `knob.trackStroke` | 2.0，轨道半径 `radius−1` | `src/ui/LookAndFeel.h:76-79` |
| `knob.fillStroke` | 3.0（仅当 pos > 0 时画填充弧） | `src/ui/LookAndFeel.h:81-89` |
| `knob.pointer` | 长 `radius*0.8`，描边 2.0，sin/cos 定位 | `src/ui/LookAndFeel.h:91-96` |
| `knob.angles` | 1.25π – 2.75π（270° 扫角，12 点方向起） | `src/ui/BandControlPanel.cpp:77-78` |
| `knob.dragStyle` | `RotaryVerticalDrag`（垂直拖拽） | `src/ui/BandControlPanel.cpp:75` |
| 数值后缀 | `" Hz"` / `" dB"` / `""` / `" %"` | `src/ui/BandControlPanel.cpp:6-11`、`src/ui/BandControlPanel.cpp:79` |

## 8. 已知不一致（整理时发现）

1. 暗色背景有两个不同值：`#101214`（`src/ui/LookAndFeel.h:28`、`src/PluginEditor.cpp:46`）vs `#14171A`（`src/ui/DisplayView.cpp:38`）。
2. `textDim`（`#9AA0A6`，LookAndFeel.h:33）与 `display.text`（`#8A9096`，DisplayView.cpp:40）在暗色下是两个不同的灰。
3. 主题色 switch 与频率标签/网格倍频表等常量分散在多文件，无集中令牌文件。
