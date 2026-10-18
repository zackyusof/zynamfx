# Building zynamfx on Windows (x64)

This guide covers building the **zynamfx** Standalone executable and VST3 plugin on Windows using Visual Studio 2022 and CMake.

---

## 1. Prerequisites

- **Windows 10 or 11 (x64)**
- **Git for Windows** (with Git on `PATH`)
- **CMake** (v3.22 or newer)
- **Visual Studio 2022** (Community, Professional, or Enterprise)
  - Workload: **Desktop development with C++** (MSVC v143 x64/x86 build tools and Windows 10/11 SDK)

---

## 2. Clone Repository & Initialize Submodules

Clone the repository recursively to ensure all dependencies (`AudioDSPTools`, `NeuralAmpModelerCore`, and nested submodules like `eigen`) are checked out:

```powershell
git clone --recurse-submodules https://github.com/zackyusof/zynamfx.git
cd zynamfx
```

If you already cloned without submodules:

```powershell
git submodule update --init --recursive
```

---

## 3. Steinberg ASIO SDK Setup (for Standalone Audio)

To support low-latency **ASIO** drivers (such as Focusrite, ASIO4ALL, Universal Audio, etc.) in the Standalone application, the **Steinberg ASIO SDK** is required.

> [!NOTE]
> **Why is the SDK not in the repository?**
> Steinberg's licensing terms prohibit redistributing the ASIO SDK files (`iasiodrv.h`, etc.) in public Git repositories. The SDK must be obtained directly from Steinberg.

An automated helper script is provided to download and extract the SDK into `libs/asiosdk/` (which is already `.gitignore`d):

### Using PowerShell:
```powershell
powershell -ExecutionPolicy Bypass -File .\script\setup-asio-sdk.ps1
```

### Using Bash / Git Bash:
```bash
bash script/setup-asio-sdk.sh
```

`plugin/CMakeLists.txt` automatically detects `libs/asiosdk/ASIOSDK` and enables `JUCE_ASIO=1`. If you place the SDK elsewhere, you can manually pass `-DASIO_SDK_PATH=<path>` during CMake configuration.

---

## 4. Configure with CMake

Configure the project for Visual Studio 2022 (x64) in Release mode:

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
```

Verify that the CMake output includes:
- `-- ASIO enabled (SDK: ...)`
- `-- CPM: Adding package JUCE@9.0.3`
- `-- Configuring done`
- `-- Generating done`

---

## 5. Build

Compile all targets in parallel:

```powershell
cmake --build build --config Release --parallel
```

---

## 6. Output Artifacts

Upon a successful build, the output binaries are located in:

| Target | Path |
| :--- | :--- |
| **Standalone Application** | `build\plugin\TONE3000_artefacts\Release\Standalone\zynamfx.exe` |
| **VST3 Plugin Bundle** | `build\plugin\TONE3000_artefacts\Release\VST3\zynamfx.vst3` |

### Installing / Using the Plugin:
- **Standalone**: Run `zynamfx.exe` directly. Open **Settings → System Settings → Audio Driver** to choose **ASIO** and select your interface.
- **VST3**: Copy the `zynamfx.vst3` folder to your system VST3 directory (typically `C:\Program Files\Common Files\VST3\`).

---

## 7. Packaging & Distribution

To create distributable release packages (`.zip` and optional Inno Setup installer `.exe`), run:

```powershell
powershell -ExecutionPolicy Bypass -File .\script\package-windows-release.ps1
```

This generates in `release/`:
- `zynamfx-<version>-windows-x64-standalone.zip` (Portable standalone app)
- `zynamfx-<version>-windows-x64-vst3.zip` (VST3 bundle + installation notes)
- `zynamfx-<version>-windows-x64-complete.zip` (Standalone + VST3 + CLAP + Factory Presets)
- `zynamfx-v<version>-windows-x64-setup.exe` (if [Inno Setup 6](https://jrsoftware.org/isinfo.php) is installed)
