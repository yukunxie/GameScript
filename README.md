
## Required Environment

- Visual Studio: **VS 2026 (preferred)** — CMake generator: `Visual Studio 18 2026`
- Fallback: VS 2022 — CMake generator: `Visual Studio 17 2022`
- CMake version: **>= 3.20.0**
- Platform: `x64`

## Quick Build (using CMake Presets)

run:

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

## Syntax highlight

install vs code extension `*.vsix` in tools\vscode-gs

![syntax-highlight](https://github.com/yukunxie/GameScript/raw/main/tools/vscode-gs/Syntax-highlight.png)


## demo

demo.gs in project gs_demo

```

...
[script] adder(10, 20) returned 40
[script] [bench] passed groups 6
[script] [bench] checksum1 499500
[script] [bench] checksum2 2001000
[script] [bench] checksum3 1499555
[script] [bench] checksum4 703
[script] [bench] checksum5 167687884
[script] [bench] checksum6 (operators) 556102
[script] [bench] gc 184
[script] [bench] suite done
[script] [script] All functional tests passed!
[script]
[script] [2/2] Running Performance Benchmarks...
[script] [DEBUG] Creating BenchmarkSuite...
[script] [DEBUG] BenchmarkSuite created, adding tests...
[script] ================================================================================
[script]                       GameScript Performance Benchmark
[script] ================================================================================
[script]
  Hot Loop (10K iterations): 9.90 ms/op  (10 iter)
  Module Calls (2K calls): 47.80 ms/op  (10 iter)
  Iterator Traversal (List/Tuple/Dict): 85.80 ms/op  (10 iter)
  Vec2 Operations (1K ops): 118.60 ms/op  (10 iter)
  Entity Creation (100 entities): 5.10 ms/op  (10 iter)
  String Operations (500 ops): 40.20 ms/op  (10 iter)
  Lambda Creation (1K lambdas): 48.80 ms/op  (10 iter)
  Operators (bitwise/logical/etc): 43.10 ms/op  (10 iter)
  Lambda Closures (complex): 42.10 ms/op  (10 iter)
  Printf Operations: 0.10 ms/op  (10 iter)
[script]
[script] --------------------------------------------------------------------------------
  Total time: 4415.00 ms
  Total operations: 100
  Average time per test: 441.50 ms
[script] ================================================================================
[script]
[script] Garbage collection reclaimed: 78 bytes
[script]
[script] [script] Demo completed successfully!
[script] __delete__ Animal Cat
[script] __delete__ Animal Dog
main() -> 0
```