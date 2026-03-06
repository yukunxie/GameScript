# GameScript Design Architecture

This document describes the current architecture of GameScript based on the repository implementation.

## 1. End-to-End Pipeline

GameScript execution follows this pipeline:

1. Source loading by `Runtime::loadSourceFile(...)`
2. Import preprocessing and source merge (`compileSourceFile`)
3. Tokenization (`Tokenizer`)
4. Parsing into AST (`Parser`)
5. Compilation from AST to IR/bytecode (`Compiler`)
6. Optional bytecode serialization (`GSBC3`)
7. Execution by `VirtualMachine`

Key files:

- `src/runtime.cpp`
- `src/compiler.cpp`
- `src/tokenizer.cpp`
- `src/parser.cpp`
- `src/vm.cpp`

## 2. Runtime Layer

`Runtime` is the host-facing entry point.

Responsibilities:

- Load `.gs` source and compile to `Module`
- Load serialized bytecode
- Hot reload source
- Call script function by name
- Persist bytecode to file

Implementation highlights:

- Runtime keeps a `std::shared_ptr<Module>` protected by `moduleMutex_`
- A fresh `VirtualMachine` instance is created per `Runtime::call(...)`
- Global host functions/modules are bound during `Runtime` construction (`bindGlobalModule`)

References:

- `include/gs/runtime.hpp`
- `src/runtime.cpp`

## 3. Frontend: Tokenizer and Parser

### 3.1 Tokenizer

Tokenizer converts source into tokens with line/column metadata.

Notable behavior:

- `#` line comments are supported
- `/* ... */` block comments are supported
- `//` is tokenized as floor-division operator (`SlashSlash`), not a comment
- Supports keywords such as `fn`, `class`, `try/catch/finally`, `throw`, `is`, `not`, `in`, `any`, `super`, `sleep`, `yield`

References:

- `include/gs/tokenizer.hpp`
- `src/tokenizer.cpp`

### 3.2 Parser

Parser builds an AST (`Program`, `FunctionDecl`, `Stmt`, `Expr`).

Supported constructs:

- Declarations: `fn`, `class`, `let`
- Control flow: `if/elif/else`, `while`, `for` (range/list/dict forms), `break`, `continue`, `return`
- Exceptions: `try/catch/finally`, `throw`, typed catch (`catch (Type as err)`)
- Expressions: binary/unary ops, calls, method calls, property/index access, assignment
- Lambda form: `(a, b) => { ... }`
- Variadic and unpack syntax:
  - parameter: `fn f(head, ...rest)`
  - call spread: `callee(rest...)`
  - multi-target unpack: `let a, b:int = source...;`

Validation examples in parser:

- Variadic parameter must be last
- Variadic parameter cannot have default value
- Unpack argument must be last in call
- Multi-target `let` requires unpack source

References:

- `include/gs/parser.hpp`
- `src/parser.cpp`

## 4. Compiler and Module Model

### 4.1 Module Structure

`Module` contains:

- constants pool
- string pool
- function bytecode list
- class bytecode list
- global bindings

Bytecode instructions use `OpCode` with slot metadata (`SlotType`) and source line/column.

References:

- `include/gs/bytecode.hpp`

### 4.2 Compiler Responsibilities

Compiler converts AST to bytecode and handles:

- symbol resolution (locals/globals/functions/classes)
- local declaration validation
- constant/default-expression evaluation for parameter defaults
- type annotation metadata storage (`paramTypeNames`, `localTypeNames`)
- lambda lowering with capture slots and closure creation
- import preprocessing and rewriting

Import preprocessing (`compileSourceFile` path):

- parses `import ...` and `from ... import ...`
- resolves modules and merges transformed source
- detects cyclic imports
- can dump transformed source to `.gst`

References:

- `include/gs/compiler.hpp`
- `src/compiler.cpp`

## 5. Bytecode and Serialization

GameScript serializes module bytecode in text format.

Current header:

- `GSBC3`

`GSBC3` includes:

- constants and strings
- functions and instructions
- type metadata
- default parameter metadata
- variadic metadata
- class/global metadata

Also supported during deserialization:

- `GSBC1`, `GSBC2` (compatibility path)

References:

- `src/compiler.cpp` (`serializeModuleText`, `deserializeModuleText`)

## 6. VM Execution Model

`VirtualMachine` executes bytecode functions using:

- `ExecutionContext`
- call `Frame` stack
- object heap and GC metadata
- module runtime global caches

Key runtime subsystems:

- Function call/dispatch (`CallValue`, `CallMethod`, spread variants)
- Exception handling (`TryBegin`, `TryEnd`, `Throw`, `MatchExceptionType`, `EndFinally`)
- Closure/upvalue operations (`CaptureLocal`, `PushCapture`, `StoreCapture`, `MakeClosure`)
- Module initialization and runtime module object caches

References:

- `include/gs/vm.hpp`
- `src/vm.cpp`

## 7. Type and Object System

Runtime values are represented by `Value`:

- Immediate: `Nil`, `Bool`, `Int`, `Float`, legacy `String` index
- Ref/object: `Ref`
- Symbols: `Function`, `Class`, `Module`

Object types include list, dict, tuple, string object, function/class/module objects, exceptions, regex, path/file, etc.

References:

- `include/gs/bytecode.hpp`
- `include/gs/type_system.hpp`
- `include/gs/type_system/*.hpp`
- `src/type_system/*.cpp`

## 8. Builtins and Modules

Global builtins are registered by `bindGlobalModule`:

- `print`, `printf`, `str`
- `Tuple`, `type`, `typename`, `id`
- `assert`
- `loadModule`
- `Exception`
- internal helper `__match_exception_type`

Built-in modules bound in host layer:

- `system` (`getTimeMs`, `gc`)
- `os` (file/path/fs operations)
- `string` (`format`, `compile` regex)

References:

- `src/global.cpp`
- `src/os_module.cpp`
- `src/string_module.cpp`

## 9. Type Annotations and Enforcement

Language supports annotations like `:int`, `:any` for:

- function parameters
- local declarations
- class attributes
- unpack targets

Behavior model in current codebase:

- compiler stores annotation metadata
- compile-time checks apply in selected constant/static scenarios (for example unpack literals)
- runtime debug checks enforce mismatches for typed assignments in debug-oriented paths

Related test scripts:

- `scripts/test_type_annotations.gs`
- `scripts/test_unpack_compile_type_mismatch.gs`
- `scripts/test_unpack_runtime_type_mismatch.gs`

## 10. Coroutines Status

Keywords exist in syntax (`spawn`, `await`, `sleep`, `yield`), but current compiler explicitly rejects `spawn/await` with temporary-disabled errors.

Implication:

- `sleep` and `yield` parse to statements
- coroutine task workflow is not enabled end-to-end yet

Reference:

- `src/compiler.cpp` (`StmtType::LetSpawn`, `StmtType::LetAwait`)

## 11. Module and Import Design Notes

Current import strategy is source-level preprocessing instead of late bytecode linking.

Pros:

- simple compile flow
- static merge and diagnostics on transformed source

Tradeoffs:

- transformed source differs from original script
- strict alias requirements exist in some `from ... import ...` cases in preprocessor rules

Reference:

- `src/compiler.cpp` (`preprocessImportsRecursive`, import parse helpers)

## 12. Diagnostics Strategy

Compiler/tokenizer/parser diagnostics carry line/column and function context suffix style:

- `line:column: error: ... [function: ...]`

Compiler also attempts to infer function context post-failure when message has `<module>`.

Reference:

- `src/tokenizer.cpp`
- `src/parser.cpp`
- `src/compiler.cpp` (`tryFillFunctionContext`)

## 13. Suggested Reading Order

1. `LANGUAGE_TUTORIAL.md`
2. `LANGUAGE_REFERENCE.md`
3. `features/PARAMETER_UNPACK_SPEC.md`
4. This architecture doc
5. `BINDING_API.md` and `BINDING_V2_GUIDE.md`
