
# build directory README

This directory contains CMake-generated build artifacts, Visual Studio project files, cache, and Debug/Release outputs.

## Required Environment

- Visual Studio: **VS 2026 (preferred)** — CMake generator: `Visual Studio 18 2026`
- Fallback: VS 2022 — CMake generator: `Visual Studio 17 2022`
- CMake version: **>= 3.20.0**
- Platform: `x64`

## Quick Build (using CMake Presets)

From the project root, run:

```powershell
# 1) Configure (VS 2026 recommended)
cmake --preset vs2026

# 2) Build Debug
cmake --build --preset vs2026-debug
```

If VS 2026 is not available, use VS 2022:

```powershell
cmake --preset vs2022
cmake --build --preset vs2022-debug
```

## Notes

- This directory is for build intermediates and outputs only.
- To regenerate, delete this directory and rerun `cmake --preset ...`.
- The root `CMakeLists.txt` enforces Windows generators: only `Visual Studio 18 2026` or `Visual Studio 17 2022` are allowed.