# Docs Overview

This page summarizes the documentation structure under `docs/`.

## Start

- `START_HERE.md`: How to use the docs portal.
- `WEBSITE_DEPLOYMENT.md`: How to publish docs as a static website.
- `LANGUAGE_TUTORIAL.md`: Step-by-step language tutorial.
- `LANGUAGE_REFERENCE.md`: Quick syntax and builtin reference.
- `DESIGN_ARCHITECTURE.md`: Compiler/runtime architecture details.

## Runtime and Language

- `features/PARAMETER_UNPACK_SPEC.md`: Variadic and unpack specification.
- `TypeObject.md`: Runtime type object model notes.
- `g1-lite-design.md`: Design notes for G1-lite.

## Binding System

- `BINDING_API.md`: Main C++ binding API guide.
- `BINDING_V2_GUIDE.md`: Binding v2 usage guide.
- `BINDING_V2_README.md`: Binding v2 overview and examples.
- `BINDING_V2_REFACTORING.md`: Migration and refactoring context.
- `BINDING_V2_CMAKE_INTEGRATION.md`: CMake integration details.
- `BINDING_V2_MIGRATION_COMPLETE.md`: Migration completion details.

## Benchmark

- `BENCHMARK_GUIDE.md`: Benchmark usage.
- `BENCHMARK_INTEGRATION.md`: Benchmark integration details.

## Maintenance Tips

- Add new Markdown files under `docs/`.
- Register new files in `docs/docs-manifest.json` so they appear in the sidebar.
- Keep paths relative to `docs/` root (for example: `features/NEW_FEATURE.md`).
