# Crunch EQ — 工程约定（CONVENTIONS）

> 只读分析产出。所有结论均附代码出处，路径相对 `Crunch EQ Source/`。
> 文档层面的声明（README.md / DESIGN.md）与代码不一致之处集中在 §8 列出。

---

## 1. 技术栈与版本

| 项目 | 取值 | 出处 |
|------|------|------|
| 语言标准 | C++20（`CMAKE_CXX_STANDARD 20`，扩展关闭） | `CMakeLists.txt:5-7` |
| 构建系统 | CMake ≥ 3.24 | `CMakeLists.txt:1` |
| 音频框架 | JUCE **8.0.4**（`FetchContent` 拉取或 `-DJUCE_DIR` 指定本地） | `CMakeLists.txt:14`、`REBUILD.txt:7`、`REBUILD.txt:18` |
| 插件外壳 | `juce_add_plugin` + `juce_generate_juce_header` | `CMakeLists.txt:30`、`CMakeLists.txt:44` |
| 参数管理 | `juce::AudioProcessorValueTreeState`（APVTS）统一管理全部参数 | `src/PluginProcessor.cpp:8` |
| DSP 库 | `juce::dsp`（FFT / IIR 系数）+ 自研 RBJ 双二阶 | `src/dsp/SpectrumAnalyzer.cpp:9`、`src/dsp/EQBand.cpp:99`、`src/dsp/BiquadFilter.h:60-218` |
| 编译选项 | MSVC: `/utf-8 /permissive-`；其他: `-Wall -Wextra` | `CMakeLists.txt:66-70` |
| 总线布局 | 立体声或单声道，输入输出必须同构 | `src/PluginProcessor.cpp:5-7`、`src/PluginProcessor.cpp:31-41` |
| MIDI | 不需要（`NEEDS_MIDI_INPUT/OUTPUT FALSE`，`acceptsMidi()==false`） | `CMakeLists.txt:39-40`、`src/PluginProcessor.h:26-28` |

插件标识（CMake 与已编译 VST3 的 moduleinfo 一致）：

| 项 | 值 | 出处 |
|----|----|------|
| PRODUCT_NAME | `Crunch EQ` | `CMakeLists.txt:31`；`Crunch EQ.vst3/Contents/Resources/moduleinfo.json:2` |
| COMPANY_NAME / BUNDLE_ID | `Kilo` / `com.kilo.cruncheq` | `CMakeLists.txt:32-33`；`moduleinfo.json:5` |
| VERSION | 1.0.0 | `CMakeLists.txt:34`；`moduleinfo.json:3` |
| Manufacturer / Plugin Code | `Kilo` / `CREQ` | `CMakeLists.txt:35-36` |
| FORMATS | VST3、Standalone | `CMakeLists.txt:37` |
| 类别 | Fx（非合成器） | `moduleinfo.json:23`、`CMakeLists.txt:38` |
| 类名 | `ProQ3Clone`（target）/ `ProQ3CloneAudioProcessorEditor`（EDITOR_NAME） | `CMakeLists.txt:30`、`CMakeLists.txt:41` |

## 2. 目录结构

按 `CMakeLists.txt:46-56` 的 `target_sources` 与仓库实际文件（头文件-only 模块无 .cpp）：

```
Crunch EQ Source/
├── CMakeLists.txt              # JUCE CMake 构建（FetchContent / JUCE_DIR）
├── README.md                   # 构建说明（文档）
├── DESIGN.md                   # 设计文档（文档，与代码有出入，见 §8）
├── .gitignore
└── src/
    ├── PluginProcessor.h/.cpp  # AudioProcessor + APVTS + 音频线程 + 分析缓冲
    ├── PluginEditor.h/.cpp     # 顶层编辑器（DisplayView + BandControlPanel 两段布局）
    ├── Parameters.h/.cpp       # 参数 ID / 枚举 / 范围 / 布局工厂
    ├── dsp/
    │   ├── BiquadFilter.h      # RBJ 双二阶（仅头文件，无 .cpp）
    │   ├── EQBand.h/.cpp       # 单频段（级联节 + 参数平滑 + 相位模式）
    │   ├── EqualizerDSP.h/.cpp # 24 频段编排 + 相位模式 + 线性相位 FIR 设计
    │   ├── LinearPhaseFilter.h/.cpp  # FIR 重叠相加卷积引擎（仅头文件+... 实际有 .cpp）
    │   ├── ColorProcessor.h    # 染色模块（仅头文件）
    │   └── SpectrumAnalyzer.h/.cpp   # FFT 频谱分析
    └── ui/
        ├── LookAndFeel.h       # 主题 + 自定义旋钮（仅头文件）
        ├── DisplayView.h/.cpp  # 频谱/曲线/频点交互/电平表
        └── BandControlPanel.h/.cpp  # 底部功能区（频段 + 染色 + 全局）
```

注：`BiquadFilter.h`、`ColorProcessor.h`、`LookAndFeel.h` 为纯头文件实现（`CMakeLists.txt:46-56` 中无对应 .cpp 条目）。

## 3. 参数 ID 命名规则

全部参数经 `Param::createParameterLayout()` 注册（`src/Parameters.cpp:61-136`），`ParameterID` 版本号一律为 `1`（`src/Parameters.cpp:65-131` 各处），APVTS 的 ValueTree 根类型为 `"PARAMETERS"`（`src/PluginProcessor.cpp:8`）。

**全局参数**：单个 camelCase 单词（`src/Parameters.h:48-58`）：

`inputGain`、`outputGain`、`phaseMode`、`linearQuality`、`gainScale`、`analyzerMode`、`analyzerTilt`、`autoGain`、`colorMode`、`colorAmount`、`colorPosition`

**频段参数**（`i = 0…23`）：字符串拼接 `"band" + i + PascalCase 字段`，由辅助函数生成（`src/Parameters.h:60-65`）：

| ID 模板 | 函数 | 类型 |
|---------|------|------|
| `band{i}Enabled` | `bandEnabled(i)` | Bool（旁路/启用） |
| `band{i}Type` | `bandType(i)` | Choice（8 种滤波类型） |
| `band{i}Freq` | `bandFreq(i)` | Float（Hz） |
| `band{i}Gain` | `bandGain(i)` | Float（dB） |
| `band{i}Q` | `bandQ(i)` | Float |
| `band{i}Slope` | `bandSlope(i)` | Choice（dB/oct） |

命名风格细节：字段名大小写不统一——`Enabled/Type/Freq/Gain/Slope` 为 PascalCase，而 `Q` 为单字母大写（`src/Parameters.h:60-65`）。

**宿主显示名**：全局为人类可读短语（`"Input Gain"` 等，`src/Parameters.cpp:66-106`）；频段为 `"Band " + (i+1) + " Enabled/Type/Freq/Gain/Q/Slope"`（1-based 显示编号，`src/Parameters.cpp:110-132`）。

**枚举定义**：`Param::FilterType`（Bell/LowShelf/HighShelf/LowCut/HighCut/Notch/BandPass/TiltShelf，`src/Parameters.h:21-32`）、`Param::PhaseMode`（`src/Parameters.h:34-39`）、`Param::ColorMode`（`src/Parameters.h:41-46`）。滤波器类型显示名见 `src/Parameters.cpp:5-17`；斜率 dB↔索引转换见 `slopeDbToIndex/slopeIndexToDb`（`src/Parameters.h:69-70`、`src/Parameters.cpp:19-39`）。

## 4. 单位与范围约定

| 参数 | 类型/范围 | 步长 | 默认 | 单位后缀 | 出处 |
|------|-----------|------|------|----------|------|
| `band{i}Freq` | 20–20000 Hz，`setSkewForCentre(1000)` 对数 | 1.0 | 1000 Hz | `"Hz"` | `src/Parameters.h:9-11`；`src/Parameters.cpp:41-46`、`src/Parameters.cpp:118-120` |
| `band{i}Gain` | −30…+30 dB | 0.01 | 0 dB | `"dB"` | `src/Parameters.h:12-14`；`src/Parameters.cpp:122-124` |
| `band{i}Q` | 0.025–40，`setSkewForCentre(1.0)` 对数 | 0.01 | 1.0 | 无 | `src/Parameters.h:15-17`；`src/Parameters.cpp:48-53`、`src/Parameters.cpp:126-128` |
| `band{i}Slope` | Choice 6/12/18/24/30/36/48 dB/oct | — | idx 1 = 12 dB/oct | — | `src/Parameters.cpp:55-59`、`src/Parameters.cpp:130-132` |
| `band{i}Type` | Choice 8 种 | — | idx 0 = Bell | — | `src/Parameters.cpp:7-16`、`src/Parameters.cpp:114-116` |
| `band{i}Enabled` | Bool | — | 全部 false | — | `src/Parameters.cpp:110-112` |
| `inputGain` / `outputGain` | −36…+36 dB | 0.1 | 0 dB | `"dB"` | `src/Parameters.h:18-19`；`src/Parameters.cpp:65-71` |
| `phaseMode` | Choice Zero Latency/Natural Phase/Linear Phase | — | idx 0 | — | `src/Parameters.cpp:73-75` |
| `linearQuality` | Choice Low/Medium/Maximum | — | idx 1 | — | `src/Parameters.cpp:77-79` |
| `gainScale` | Choice 3/6/12/30 dB | — | idx 2 = 12 dB | — | `src/Parameters.cpp:81-83` |
| `analyzerMode` | Choice Off/Pre/Post/Pre + Post | — | idx 3 = Pre+Post | — | `src/Parameters.cpp:85-87` |
| `analyzerTilt` | 0–4.5 dB/oct | 0.1 | 4.5 | `"dB/oct"` | `src/Parameters.cpp:89-91` |
| `autoGain` | Bool | — | false（预留：无 UI、DSP 未使用） | — | `src/Parameters.cpp:93-94` |
| `colorMode` | Choice Off/Warm/Cold/Clip | — | idx 0 | — | `src/Parameters.cpp:96-98` |
| `colorAmount` | 0–100 % | 0.1 | 0 | `"%"`（DSP 内 /100 归一化） | `src/Parameters.cpp:100-102`；`src/PluginProcessor.cpp:104` |
| `colorPosition` | Choice Pre/Post | — | idx 1 = Post | — | `src/Parameters.cpp:104-106` |

约定：频率与 Q 均用 `NormalisableRange::setSkewForCentre` 做对数手感（`src/Parameters.cpp:41-53`）；枚举类参数（Choice/Bool）在 DSP 侧以 `std::atomic<float>` 读取后 `(int)` 转换（如 `src/PluginProcessor.cpp:81`、`src/PluginProcessor.cpp:122-124`）。

## 5. 预设格式与路径

- **没有预设系统**：全仓库无 `preset` 字样（grep 无结果）；`getNumPrograms() == 1`，`get/setCurrentProgram`、`getProgramName` 均为空实现（`src/PluginProcessor.h:31-35`）。
- **状态持久化**（宿主会话级）：`getStateInformation` 将 `apvts.copyState()` 生成 `ValueTree` → XML → `copyXmlToBinary` 写入（`src/PluginProcessor.cpp:246-251`）；`setStateInformation` 用 `getXmlFromBinary` 解析并 `replaceState`（`src/PluginProcessor.cpp:253-258`）。无独立预设文件路径。
- **未入持久化状态的非参数状态**：`theme_`、`editorWidth_/editorHeight_`、`selectedBand_` 均为 processor 内的 `std::atomic`（`src/PluginProcessor.h:73-78`），只随进程内存存活；`getStateInformation` 不含它们（`src/PluginProcessor.cpp:246-251`），因此宿主重开工程后主题与窗口尺寸回到默认（详见 §7、§8）。

## 6. 构建 / 打包方式

```powershell
# 本地 JUCE（离线）
cmake -B build -A x64 -DJUCE_DIR="<JUCE-8.0.4 目录>" "Crunch EQ Source"
cmake --build build --config Release
# 或联网：省略 -DJUCE_DIR，由 FetchContent 拉取 JUCE 8.0.4（CMakeLists.txt:12-23）
```

- 产物路径：`build\ProQ3Clone_artefacts\Release\VST3\Crunch EQ.vst3`（`REBUILD.txt:22-23`；target 名 `ProQ3Clone`，`CMakeLists.txt:30`）。
- 仓库内附带预编译 VST3（`Crunch EQ.vst3/`，`moduleinfo.json` 中 SDKVersion VST 3.7.12，`moduleinfo.json:21`）。
- 链接库：`juce_audio_plugin_client`、`juce_audio_devices`、`juce_audio_utils`、`juce_dsp`、`juce_recommended_config_flags`（`CMakeLists.txt:58-64`）。
- 未设置 `COPY_PLUGIN_AFTER_BUILD`（CMakeLists 中无此项；README.md:32 仅将其列为可选项）。

## 7. 编辑器尺寸与缩放策略

- 默认尺寸 920×620，可自由缩放，限制 230×155 ~ 1840×1240（`src/PluginEditor.cpp:17-19`）。
- 布局两段式：底部 `BandControlPanel` 固定高 240px，`DisplayView` 占剩余全部（`src/PluginEditor.cpp:53-55`）。
- 尺寸记忆：每次 `resized()` 调用 `processor_.setEditorSize()`（`src/PluginEditor.cpp:51`），构造时读回（`src/PluginEditor.cpp:15-17`）——仅存于 atomic，不写入 APVTS 状态（见 §5）。
- **无 DPI 缩放 / 无全局缩放因子**（代码中无 `setGlobalScaleFactor` 之类调用）。缩放策略是纯相对布局：
  - 面板 3 行等高（`area.getHeight() / 3`，`src/ui/BandControlPanel.cpp:130-133`）；
  - 旋钮区按行剩余宽度 3 等分（频段行，`BandControlPanel.cpp:146-149`）或 2 等分（全局行，`BandControlPanel.cpp:162-164`）；
  - 组合框为**固定像素宽**（90/90/64、88/64/90、100/80/80/80，`BandControlPanel.cpp:141-159`）；
  - 字体字号固定（12/11/10/9，`src/ui/DisplayView.cpp:380`、`404`、`586`、`590`）。
- 边距约定：面板整体 `reduced(8)`，行间留 6px，各控件 `reduced(2)`（`BandControlPanel.cpp:129`、`144`、`160` 及各处 `setBounds` 链）。

## 8. 文档与代码不一致点（以代码为准）

| # | 文档声明 | 代码实际 | 出处 |
|---|----------|----------|------|
| 1 | "双击频点删除该频段"（README.md:11、DESIGN.md:168-169） | 双击仅在**空白处新增**频点；频点上双击无任何行为。删除只能**右键**频点 | 双击：`src/ui/DisplayView.cpp:331-339`；右键删除：`src/ui/DisplayView.cpp:205-211` |
| 2 | "仅频段 0 默认启用"（DESIGN.md:96） | 24 个 `band{i}Enabled` 默认**全部 false** | `src/Parameters.cpp:110-112` |
| 3 | 频段增益范围 ±18 dB（DESIGN.md:99） | 实际 ±30 dB | `src/Parameters.h:12-13` |
| 4 | "空白处拖拽 = 框选（预留）"（DESIGN.md:170） | 空白处拖拽无行为（`mouseDrag` 在未命中频点时直接返回） | `src/ui/DisplayView.cpp:225-233`、`src/ui/DisplayView.cpp:238-239` |
| 5 | `Param::ColorMode` 枚举只有 Off/Warm/Cold | 参数 Choice 实际有 4 项（含 Clip），DSP 也实现 mode 3 | 枚举：`src/Parameters.h:41-46`；Choice：`src/Parameters.cpp:98`；DSP：`src/dsp/ColorProcessor.h:43-49` |
| 6 | 刷新 ~30–60 Hz（DESIGN.md:176） | 固定 60 Hz | `src/ui/DisplayView.cpp:7` |
