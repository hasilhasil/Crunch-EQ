# ProQ3Clone — 数字参数均衡器（VST3）设计文档

一款界面与交互对标 FabFilter Pro-Q 3 的数字 EQ 音频插件，VST3 格式。
本仓库交付：**完整设计文档 + 可编译运行的 JUCE 工程脚手架**（含核心 DSP 与 UI 框架）。

---

## 1. 目标与非目标

### 1.1 目标（本脚手架已覆盖）
- VST3（另含 AU / Standalone）插件外壳，基于 JUCE。
- 最多 24 个 EQ 频段，每个频段至少具备：**频率选择 / 增益 / Q 值** 三大核心参数，外加滤波类型、斜率、旁路。
- 滤波类型：Bell（峰值）、Low/High Shelf（搁架）、Low/High Cut（高低切）、Notch（陷波）、Band Pass（带通）、Tilt Shelf（倾斜）。
- 三种相位模式：**Zero Latency（零延迟）**、**Natural Phase（自然相位）**、**Linear Phase（线性相位）**。
- 音频频率可视化：输入/输出频谱分析仪（FFT）+ EQ 响应曲线实时叠加。
- 交互：在频谱图上点击新增频点、拖拽调频/增益、滚轮调 Q、双击删除，界面风格接近 Pro-Q 3。

### 1.2 非目标（路线图，脚手架未实现）
- 动态 EQ（Dynamic EQ，即 Pro-Q 3 的 bell 动态模式）。
- 每频段 L/R 与 Mid/Side 处理模式（脚手架为真立体声，双通道共享系数）。
- GPU（OpenGL）加速曲线绘制（当前用 CPU 抗锯齿矢量绘制，足够 60fps）。
- 精确的 AAX / Linux VST3 打包（代码兼容，但未在此环境验证）。
- 自动化测试（DSP 单元测试、快照对比）——见 §8 路线图。

---

## 2. 技术选型与理由

| 维度 | 选择 | 理由 |
|------|------|------|
| 框架 | **C++ / JUCE 8** | 行业事实标准；FabFilter Pro-Q 3 本身即基于 JUCE。VST3/AU 封装、`AudioProcessorValueTreeState`（参数/自动化/状态持久化）、`juce_dsp`（FFT/IIR）、矢量 UI 全部内置，跨平台。 |
| 构建 | **CMake + `juce_add_plugin`** | JUCE 官方推荐的现代构建方式，配合 `FetchContent` 或本地 `JUCE_DIR`。 |
| 最小相位滤波 | **RBJ Audio EQ Cookbook 双二阶（biquad）** | 零延迟模式的标准实现；计算量小、参数化好、被广泛验证。 |
| 自然相位滤波 | **JUCE 模拟原型匹配 IIR 系数**（`juce::dsp::IIR::Coefficients`） | 与 Pro-Q 3「Natural Phase」一致：匹配模拟原型幅频响应（最小相位）。 |
| 线性相位滤波 | **频率采样法设计 FIR + 重叠相加（overlap-add）FFT 卷积** | 幅频响应与最小相位模式一致，相位严格线性；通过改变 FFT 长度实现 Low/Medium/Maximum 三档（时延/精度权衡）。 |
| 频谱分析 | **juce::dsp::FFT + Hann 窗 + EMA 平滑 + 对数频率重映射 + 倾斜补偿** | 标准做法，匹配 Pro-Q 3 的倾斜频谱（tilt）显示。 |

---

## 3. 系统架构

```
┌───────────────────────────────────────────────────────────────┐
│                        PluginEditor (UI)                      │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │ DisplayView : 频谱 + EQ 曲线 + 频点拖拽/新增/删除          │  │
│  └─────────────────────────────────────────────────────────┘  │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │ BandControlPanel : 选中频段的 频率/增益/Q 旋钮、类型/斜率、  │  │
│  │                    全局(相位模式/增益刻度/分析仪/IO增益)     │  │
│  └─────────────────────────────────────────────────────────┘  │
│        ▲ 读参数/写参数(值树监听)   ▲ 拉取频谱/响应曲线          │
└────────┼───────────────────────────┼─────────────────────────┘
         │ AudioProcessorValueTreeState (线程安全的参数树)
┌────────┴───────────────────────────┴─────────────────────────┐
│                     PluginProcessor (DSP)                     │
│  processBlock()                                                │
│   ├─ 读原始参数值 (getRawParameterValue, 原子)                  │
│   ├─ 更新 EqualizerDSP 各频段 / 相位模式 / 线性相位重建          │
│   ├─ 输入/输出增益                                             │
│   ├─ 缓存 pre/post 单声道用于分析仪                             │
│   └─ 上报时延 (setLatency)                                     │
│      │                                                         │
│      ▼                                                         │
│  EqualizerDSP ─┬─ EQBand[0..23] (RBJ biquad / JUCE IIR 级联)   │
│                └─ LinearPhaseFilter (频率采样 FIR + overlap-add)│
└────────────────────────────────────────────────────────────────┘
```

**关键设计原则**
- 音频线程（`processBlock`）只读 `std::atomic<float>` 原始参数值 + 本地缓存，绝不做锁/分配。
- UI 线程通过 `AudioProcessorValueTreeState` 监听参数变化触发重绘；通过加锁缓冲区拉取最近一帧音频做 FFT。
- 响应曲线由最小相位 biquad 的频响函数解析计算得到（三种相位模式幅频一致，故曲线只画一条）。

---

## 4. 参数模型

所有参数经 `AudioProcessorValueTreeState` 管理，自动获得：自动化、宿主会话保存/恢复、UI 绑定。

### 4.1 全局参数
| ID | 类型 | 范围 | 默认 |
|----|------|------|------|
| `inputGain` | Float | −36…+36 dB | 0 |
| `outputGain` | Float | −36…+36 dB | 0 |
| `phaseMode` | Choice | Zero Latency / Natural Phase / Linear Phase | Zero Latency |
| `linearQuality` | Choice | Low / Medium / Maximum | Medium |
| `gainScale` | Choice | 3 / 6 / 12 / 30 dB | 12 dB |
| `analyzerMode` | Choice | Off / Pre / Post / Pre+Post | Pre+Post |
| `analyzerTilt` | Float | 0…4.5 dB/oct | 4.5 |
| `autoGain` | Bool | — | false（预留） |

### 4.2 每频段参数（`i = 0…23`）
| ID | 类型 | 范围 | 默认 |
|----|------|------|------|
| `band{i}Enabled` | Bool | — | 仅频段 0 为 true |
| `band{i}Type` | Choice | Bell/LowShelf/HighShelf/LowCut/HighCut/Notch/BandPass/TiltShelf | Bell |
| `band{i}Freq` | Float | 20…20000 Hz（对数偏斜，中心 1 kHz） | 1000 |
| `band{i}Gain` | Float | −18…+18 dB | 0 |
| `band{i}Q` | Float | 0.025…40（对数偏斜） | 1.0 |
| `band{i}Slope` | Choice | 6/12/18/24/30/36/48 dB/oct | 12 |

> 频率与 Q 使用 `NormalisableRange::setSkewForCentre` 使旋钮/拖拽手感对数化，符合听觉感知。

---

## 5. DSP 设计

### 5.1 Biquad（`BiquadFilter`）
- 结构：**Transposed Direct Form II**，差分方程
  `y[n] = b0·x[n] + b1·x[n-1] + b2·x[n-2] − a1·y[n-1] − a2·y[n-2]`。
- 系数由 RBJ Audio EQ Cookbook 生成：peaking、low/high shelf、HPF/LPF、notch、constant-0dB bandpass。
- 6 dB/oct 用一阶（双线性变换）节；12 dB/oct 及以上用二阶节级联。
- 提供 `responseAt(freq)`：把 `z = e^{−jω}` 代入传递函数
  `H(z) = (b0 + b1 z⁻¹ + b2 z⁻²) / (1 + a1 z⁻¹ + a2 z⁻²)`，用于绘制响应曲线。

### 5.2 EQBand（`EQBand`）
- 内部持有「一节或多节 biquad」，由类型 + 斜率决定节数与结构：
  - Bell/Shelf/Notch/BandPass：1 节；Q 直接映射。
  - LowCut/HighCut：按 slope（6→1 个一阶节，12→1 个二阶节，24→2 个二阶节，…，48→4 个二阶节）。
  - TiltShelf：低搁架(−g/2) + 高搁架(+g/2) 两节近似。
- `magnitudeAt(freq)`：各节 `|responseAt|` 连乘（线性幅度），用于响应曲线与线性相位 FIR 设计目标。

### 5.3 相位模式
| 模式 | 实现 | 时延 |
|------|------|------|
| Zero Latency | RBJ 双二阶级联（数字域直接设计） | 0 |
| Natural Phase | `juce::dsp::IIR::Coefficients` 模拟原型匹配 IIR（最小相位，幅频匹配模拟原型） | 0 |
| Linear Phase | 频率采样法 FIR + overlap-add 卷积 | `block + (L−1)/2` |

### 5.4 线性相位（`LinearPhaseFilter`）
1. **FIR 设计（频率采样法）**：在 FFT 网格 `k=0…N/2` 上采样目标幅度
   `A[k] = Π |H_band(f_k)|`，构造对称线性相位谱
   `H[k] = A[k]·e^{−jπk(L−1)/N}`，用共轭对称补齐 `k>N/2`，IFFT 取实部得冲激响应 `h[n]`（长度 L，奇数）。
2. **卷积**：重叠相加（overlap-add）。分块大小 `B = N/2`，`L ≤ B`；输入/输出各用 FIFO 缓冲，适配宿主任意 `numSamples`。
3. **时延**：`latency = B + (L−1)/2`，经 `getLatencySamples()` 上报，宿主自动做延迟补偿。
4. 三档质量：Low(N=2048) / Medium(N=4096) / Maximum(N=8192)。
   - 注意：当前脚手架的重叠相加引入了一个 `B` 的分块时延；生产级实现可用零时延分区卷积（partitioned convolution）把该分量消除，仅保留 `(L−1)/2` 的线性相位群时延。见 §8。

### 5.5 频谱分析仪（`SpectrumAnalyzer`）
- 拉取最近一帧单声道音频 → Hann 窗 → FFT → `20·log10|X|`（dB）。
- EMA 平滑（时间平均）+ 峰值保持（可选）。
- 倾斜补偿：`tilt · log2(f/f_ref)` 叠加，模拟 Pro-Q 3 的倾斜频谱。
- 输出映射到对数频率网格（如 10 Hz–20 kHz，~512 点，线性 bins 插值）。

---

## 6. UI 设计

### 6.1 布局（对标 Pro-Q 3）
```
┌────────────────────────────────────────────────────┐
│  DisplayView（频谱 + EQ 曲线 + 频点 + 网格 + 增益刻度）│
├────────────────────────────────────────────────────┤
│  BandControlPanel                                    │
│   [类型][斜率]   (Freq 旋钮)(Gain 旋钮)(Q 旋钮)  [旁路]│
│   [相位模式][线性质量][增益刻度][分析仪][InGain][OutGain]│
└────────────────────────────────────────────────────┘
```

### 6.2 DisplayView 交互
| 操作 | 行为 |
|------|------|
| 双击空白处 | 新增一个 Bell 频段（默认 0 dB） |
| 单击频点 | 选中该频段（底部面板随之绑定） |
| 拖动频点 | 水平=频率（对数），垂直=增益（按 gainScale 映射） |
| 拖动 + Ctrl | 仅调 Q（垂直方向） |
| 滚轮在频点上 | 调 Q |
| 双击频点 | 删除该频段 |
| 空白处拖拽 | 框选（预留） |

### 6.3 绘制
- 深色主题自定义 `LookAndFeel`（`ui/LookAndFeel.h`），自定义 `drawRotarySlider` 旋钮。
- 曲线用 `juce::Path` + 抗锯齿绘制；频谱用渐变填充 `juce::ColourGradient`。
- 网格/刻度随 `gainScale` 动态变化。
- 刷新策略：`juce::Timer`（~30–60 Hz）拉取频谱数据 + 响应曲线并 `repaint()`。

---

## 7. 项目结构

```
ProQ3Clone/
├── CMakeLists.txt            # JUCE CMake 构建（FetchContent / JUCE_DIR）
├── README.md                 # 构建与运行说明
├── DESIGN.md                 # 本文档
├── .gitignore
└── src/
    ├── PluginProcessor.h/.cpp    # AudioProcessor + APVTS + 音频线程
    ├── PluginEditor.h/.cpp       # 顶层编辑器
    ├── Parameters.h/.cpp         # 参数 ID / 枚举 / 布局工厂
    ├── dsp/
    │   ├── BiquadFilter.h        # RBJ 双二阶
    │   ├── EQBand.h/.cpp         # 单频段（级联节 + 幅频）
    │   ├── EqualizerDSP.h/.cpp   # 频段管理 + 相位模式 + 线性相位重建
    │   ├── LinearPhaseFilter.h/.cpp  # FIR 设计 + overlap-add 卷积
    │   └── SpectrumAnalyzer.h/.cpp   # FFT 分析仪
    └── ui/
        ├── LookAndFeel.h         # 深色主题 + 自定义旋钮
        ├── DisplayView.h/.cpp    # 频谱/曲线/交互
        └── BandControlPanel.h/.cpp # 频段 + 全局控制
```

---

## 8. 实现计划 / 路线图

| 阶段 | 内容 | 状态 |
|------|------|------|
| 0 | 项目骨架 + 参数模型 + 构建通过 | ✅ 本脚手架 |
| 1 | 最小相位 EQ（RBJ + 模拟 IIR） | ✅ |
| 2 | 线性相位 FIR + overlap-add | ✅ |
| 3 | 频谱分析仪 + 响应曲线 | ✅ |
| 4 | 交互式 UI（频点拖拽/新增/删除） | ✅ |
| 5 | 动态 EQ（bell 动态检测/压缩） | 待实现 |
| 6 | L/R、Mid/Side 每频段模式 | 待实现 |
| 7 | 零时延分区卷积（消除分块时延） | 待实现 |
| 8 | 自动化测试（DSP 快照、频响断言） | 待实现 |
| 9 | OpenGL 曲线渲染 + AAX/Linux 打包 | 待实现 |
