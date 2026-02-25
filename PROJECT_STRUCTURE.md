# GameScript Project

A lightweight scripting language with C++ performance and Python-like syntax.

## Project Structure

```
GameScript/
├── app/                      # Application executables
├── build/                    # Build output directory
├── docs/                     # Documentation
├── include/                  # Header files
│   └── gs/                  # GameScript core headers
├── scripts/                  # GameScript files (*.gs)
│   ├── tests/               # Test suite files
│   ├── misc/                # Miscellaneous examples
│   ├── object/              # OOP examples
│   ├── demo.gs              # Main benchmark suite
│   └── module_*.gs          # Reusable modules
├── src/                      # C++ source implementation
├── tools/                    # Development tools
├── CMakeLists.txt           # Build configuration
└── OPERATOR_IMPLEMENTATION_REPORT.md  # Operator documentation
```

## Quick Start

### Build
```bash
cmake --preset default
cmake --build build --config Release
```

### Run Demo
```bash
build\bin\Release\gs_demo.exe scripts\demo.gs
```

### Run Tests
```bash
build\bin\Release\gs_demo.exe scripts\tests\test_operators.gs
```

## Features

- **Rich Operator Set**: Arithmetic (`+`, `-`, `*`, `/`, `//`, `%`, `**`), Bitwise (`&`, `|`, `^`, `~`, `<<`, `>>`), Logical (`&&`, `||`)
- **Membership Testing**: `in`, `not in` for lists, tuples, dicts, and objects
- **Identity Operators**: `is`, `not is` for reference comparison
- **OOP Support**: Classes, inheritance, polymorphism
- **Closures & Lambdas**: First-class functions with lexical scoping
- **Module System**: Import/export mechanism
- **Performance**: Stack-based VM with register optimizations

## Documentation

- See `scripts/README.md` for script organization
- See `OPERATOR_IMPLEMENTATION_REPORT.md` for operator details
- See `docs/` for language specification

## License

[License information to be added]
