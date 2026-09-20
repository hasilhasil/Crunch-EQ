# Crunch EQ — 可复用性分析（Reusability）

> 只读分析产出。结论全部附代码出处（路径相对 `Crunch EQ Source/`）。
> 判定标准：文件内是否出现 `Param::` 命名空间（参数域模型）、`FilterType/PhaseMode`（EQ 语义）、
> 或 EQ 特有的绘制/计算（频段节点、响应曲线、24 频段编排）。

## 1. 已具备通用性、可复用的部分

这些类**不含任何 EQ 专属语义**，可以直接搬到其他音频插件工程：

| 模块 | 内容 | 可复用性佐证 |
|------|------|--------------|
| `BiquadFilter`（`src/dsp/BiquadFilter.h`） | TDF-II 双二阶 + 9 个 RBJ 系数设计静态函数 + `responseAt` 频响求值 | 类体只用 `juce::MathConstants` 与 `std::complex`，无 `Param::` 引用（`src/dsp/BiquadFilter.h:15-224`）。注意：文件头有多余的 `#include "../Parameters.h"`（`src/dsp/BiquadFilter.h:7`）但未使用其内容，属于可清理的耦合 |
| `SpectrumAnalyzer`（`src/dsp/SpectrumAnalyzer.h/.cpp`） | 通用 FFT 分析仪：Hann 窗 + 50% 重叠 + EMA 平滑 + 倾斜补偿 + 对数频率重采样 | 全文件无 `Param::`/EQ 引用（`src/dsp/SpectrumAnalyzer.h:8-33`；窗/重叠/EMA 见 `src/dsp/SpectrumAnalyzer.cpp:13`、`44-49`、`71-76`） |
| `LinearPhaseFilter`（`src/dsp/LinearPhaseFilter.h/.cpp`） | 通用 FFT 重叠相加卷积引擎，接受任意冲激响应（`setImpulseResponse`） | 无 EQ 引用（`src/dsp/LinearPhaseFilter.h:8-40`；卷积主循环 `src/dsp/LinearPhaseFilter.cpp:64-113`） |
| `ColorProcessor`（`src/dsp/ColorProcessor.h`） | 通用染色引擎：tanh 驱动 + 干湿混合（mode/amount 接口通用） | 无 EQ 引用（`src/dsp/ColorProcessor.h:7-90`）；仅固定滤波器预设写死（`src/dsp/ColorProcessor.h:70-83`），机制本身通用 |
| `ProQLookAndFeel`（`src/ui/LookAndFeel.h`） | 通用深/浅主题 + 旋钮绘制（`drawRotarySlider` 覆盖） | 绘制逻辑只依赖 JUCE colour ID，无 EQ 语义（`src/ui/LookAndFeel.h:62-97`）；类名带 "ProQ" 但实现通用 |

## 2. EQ 专属、不可复用的部分

| 模块 | EQ 专属内容 | 出处 |
|------|-------------|------|
| `Param` 命名空间（`src/Parameters.h/.cpp`） | `FilterType` 8 种滤波类型枚举、`kMaxBands=24`、`band{i}Xxx` 参数 ID 方案、斜率 dB↔索引映射 | `src/Parameters.h:7-71`；`src/Parameters.cpp:19-39`、`src/Parameters.cpp:61-136` |
| `EQBand`（`src/dsp/EQBand.h/.cpp`） | 参数化频段语义（type/freq/gain/Q/slope + 5ms 平滑 + 每频段相位模式 + 静态 `magnitudeAt` 分析）；`designRbjSections` 按斜率级联节、TiltShelf=两搁架 | `src/dsp/EQBand.h:13-72`；`src/dsp/EQBand.cpp:55-90`、`src/dsp/EQBand.cpp:150-291` |
| `EqualizerDSP`（`src/dsp/EqualizerDSP.h/.cpp`） | 24 频段编排 + 三种相位模式 + **由 EQ 幅频乘积设计线性相位 FIR**（`rebuildLinearPhase`）+ 时延上报 | `src/dsp/EqualizerDSP.h:33-40`；`src/dsp/EqualizerDSP.cpp:100-133`、`src/dsp/EqualizerDSP.cpp:89-98` |
| `DisplayView`（`src/ui/DisplayView.h/.cpp`） | EQ 响应曲线绘制（连乘各频段 `magnitudeAt`）、频段节点绘制/命中/拖拽/新增/删除、随 gainScale 变化的网格与刻度、输入/输出频谱叠加 | `src/ui/DisplayView.cpp:459-513`（曲线）、`src/ui/DisplayView.cpp:515-553`（节点）、`src/ui/DisplayView.cpp:352-396`（网格） |
| `BandControlPanel`（`src/ui/BandControlPanel.h/.cpp`） | 具体 EQ 控件集（freq/gain/Q/type/slope/bypass + phase/quality/scale/analyzer/color）；"选中频段重绑 attachment"是通用 idiom，但控件集合本身是 EQ 专属 | `src/ui/BandControlPanel.h:21-29`；`src/ui/BandControlPanel.cpp:86-125` |
| `PluginProcessor` / `PluginEditor`（`src/PluginProcessor.h/.cpp`、`src/PluginEditor.h/.cpp`） | 应用胶水：EQ+染色+分析的信号流编排、分析环形缓冲、主题/尺寸/选中频段的 atomic 状态 | `src/PluginProcessor.cpp:132-222`；`src/PluginEditor.cpp:44-56` |

## 3. "抽成可复用基类"的实际情况

**仓库中没有定义任何自定义基类、抽象接口或模板化的 UI 组件**（无 `virtual` 基类、无 `Component` 子类层次复用）。
现有复用方式是**组合 + 直接实例化**：

- 频谱分析能力：`DisplayView` 直接持有两个 `SpectrumAnalyzer` 实例（`src/ui/DisplayView.h:66-67`，`src/ui/DisplayView.cpp:92-93`）——这是唯一被两处共享的通用模块。
- 旋钮外观：没有 `CustomKnob` 组件，全部集中在 `ProQLookAndFeel::drawRotarySlider`（`src/ui/LookAndFeel.h:62-97`），通过 `setLookAndFeel` 全局生效（`src/PluginEditor.cpp:9`）。
- `DisplayView` 与 `BandControlPanel` 之间零代码共享，仅通过 `std::function` 回调通信（`src/ui/DisplayView.h:29-30`，`src/PluginEditor.cpp:29-30`）。

## 4. 应当抽出而未抽出的重复实现（分析发现）

| 重复点 | 两处代码 | 出处 |
|--------|----------|------|
| 主题 accent switch | `accentForTheme()` 与 `getThemeAccent()` 各写一遍同样的三路 switch | `src/ui/LookAndFeel.h:13-21` vs `src/PluginProcessor.cpp:236-244` |
| 选择项字面量列表 | slope/phase/gainScale/analyzer/colorMode/colorPosition 的 StringArray 在参数定义与 UI 各写一遍 | `src/Parameters.cpp:57-58`、`75`、`83`、`87`、`98`、`106` vs `src/ui/BandControlPanel.cpp:23-24`、`27`、`33`、`36`、`39`、`42` |
| EQ 总幅频计算 | `EqualizerDSP::getMagnitudeDbAt` 已存在，但 `DisplayView::updateResponseCurve` 未调用它，自己重写了 24 频段连乘循环 | `src/dsp/EqualizerDSP.cpp:84-87` vs `src/ui/DisplayView.cpp:467-493` |
| 暗色背景字面量 | `#101214` 与 `#14171A` 并存 | `src/ui/LookAndFeel.h:28`、`src/PluginEditor.cpp:46` vs `src/ui/DisplayView.cpp:38` |

## 5. 性能观察（与复用性相关的隐患）

- `DisplayView::paint` 内直接调用 `updateResponseCurve()`（`src/ui/DisplayView.cpp:604`），在 60 Hz 下每帧执行 320 点 × 24 频段的 `designRbjSections`（`src/ui/DisplayView.cpp:467-489`），而该函数每次都 `return` 一个堆分配 `std::vector<BiquadCoefficients>`（`src/dsp/EQBand.cpp:55-90`）——即每帧约 7680 次 vector 分配 + biquad 复数频响求值（`src/dsp/BiquadFilter.h:45-58`），全在消息线程。
- 若将来把 `EqualizerDSP::getMagnitudeDbAt` 抽给 UI 使用（见 §4），应同时在音频线程外缓存或预计算曲线点，而不是在 `paint` 里重算。
