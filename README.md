# ProQ3Clone

界面与交互对标 FabFilter Pro-Q 3 的数字参数均衡器（VST3 / AU / Standalone），基于 C++ / JUCE 8。
完整设计见 [DESIGN.md](DESIGN.md)。

## 功能
- 最多 24 个 EQ 频段：Bell / Low·High Shelf / Low·High Cut / Notch / Band Pass / Tilt Shelf。
- 每频段：频率、增益、Q、斜率（6–48 dB/oct）、旁路。
- 相位模式：Zero Latency / Natural Phase / Linear Phase（线性相位支持 Low/Medium/Maximum 三档）。
- 输入/输出频谱可视化 + 实时 EQ 响应曲线。
- 交互：双击新增频点、拖拽调频/增益、Ctrl+拖拽或滚轮调 Q、双击删除。

## 依赖
- CMake ≥ 3.24
- C++20 编译器（MSVC 2019+ / Clang / GCC 10+）
- JUCE 8（自动经 FetchContent 拉取，或用 `-DJUCE_DIR` 指定本地路径）

## 构建（Windows / Visual Studio）
```powershell
cmake -B build -DJUCE_DIR="C:/path/to/JUCE"          # 或省略 JUCE_DIR 自动拉取
cmake --build build --config Release
```
生成产物位于 `build/ProQ3Clone_artefacts/Release/VST3/`。

## 构建（macOS，产出 AU + VST3 + Standalone）
```bash
cmake -B build -DJUCE_DIR=/path/to/JUCE
cmake --build build --config Release
```

## 运行
- VST3：将 `ProQ3Clone.vst3` 复制到宿主可识别的 VST3 目录，或在 JUCE 构建时开启 `COPY_PLUGIN_AFTER_BUILD`。
- Standalone：直接运行构建出的独立可执行文件，可选用系统音频设备实时试听。

## 结构
```
src/
├── PluginProcessor.h/.cpp     DSP 主处理 + 参数树 + 时延上报
├── PluginEditor.h/.cpp        顶层编辑器
├── Parameters.h/.cpp          参数 ID / 枚举 / 布局工厂
├── dsp/                       Biquad / EQBand / EqualizerDSP / LinearPhaseFilter / SpectrumAnalyzer / SpectrumEngine
└── ui/                        LookAndFeel / DisplayView / BandControlPanel
```

## 第三方资源
- 界面字体 Poppins（`src/ui/Fonts/`）为 SIL Open Font License 1.1，许可证全文见
  [`src/ui/Fonts/OFL.txt`](src/ui/Fonts/OFL.txt)。
- JUCE 不在本仓库内，由 CMake `FetchContent` 自动拉取（或通过 `-DJUCE_DIR` 指定本地副本）。
