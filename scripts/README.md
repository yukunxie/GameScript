# Scripts Directory Structure

This directory contains all GameScript (*.gs) example files and test cases, organized by category.

## Directory Layout

### `/` (Root)
- **demo.gs** - Main benchmark suite showcasing all language features
- **demo.py** - Python reference implementation for comparison
- **module_*.gs** - Reusable module files (imported by tests)
- **compare_demo_instruction_counts.py** - Analysis script for instruction optimization
- **demo_gs_py_instruction_comparison.md** - Instruction comparison documentation

### `/tests/`
Contains all test files for validating language features:

#### Operator Tests
- **test_operators.gs** - Comprehensive operator test suite
- **test_simple_ops.gs** - Basic operator tests
- **test_ops_bench.gs** - Operator performance benchmarks
- **test_checksum.gs** - Operator checksum validation
- **verify_checksum.gs** - Checksum verification utility
- **test_membership.gs** - `in` operator tests
- **test_not_in.gs** - `not in` operator tests

#### Language Feature Tests
- **lambda_test.gs** - Lambda expression tests
- **lambda_scope_test.gs** - Lambda scoping tests
- **lambda_toplevel_test.gs** - Top-level lambda tests
- **unary_test.gs** - Unary operator tests
- **callable_field_dispatch_test.gs** - Field dispatch tests

#### Comment Syntax Tests
- **comment_test.gs** - General comment parsing
- **block_comment_test.gs** - Multi-line comment tests
- **slash_comment_test.gs** - Single-line comment tests
- **no_comment_test.gs** - Code without comments

#### Module System Tests
- **module_class_test.gs** - Class import/export tests
- **module_globals_test.gs** - Global variable sharing tests
- **module_import_test.gs** - Module import mechanism tests

#### Object Lifecycle Tests
- **delete_hook_test.gs** - Object deletion hook tests
- **delete_hook_optional_test.gs** - Optional deletion hook tests

### `/misc/`
Miscellaneous examples and utility scripts:
- **flow.gs** - Control flow examples

### `/object/`
Object-oriented programming examples:
- **creatures.gs** - Class hierarchy and inheritance examples

## Usage

Run the main demo:
```bash
gs_demo.exe scripts/demo.gs
```

Run individual tests:
```bash
gs_demo.exe scripts/tests/test_operators.gs
```

## File Naming Conventions

- **demo*.gs** - Benchmark and demonstration files
- **test_*.gs** - Unit test files
- **module_*.gs** - Importable module files
- ***_test.gs** - Feature-specific test files
