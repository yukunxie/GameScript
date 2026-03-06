# GameScript Language Reference (Current)

This is a concise reference for quick lookup.

## 1. Keywords

`fn class extends let for in if elif else try catch finally throw while break continue str return spawn await sleep yield super as any global builtin is not true false null`

## 2. Literals

- Integer: `123`
- Float: `3.14`
- String: `"text"`
- Bool: `true`, `false`
- Null: `null`
- List: `[1, 2, 3]`
- Dict: `{1: 10, 2: 20}`

## 3. Declarations

### 3.1 Variable

```gs
let x = 1;
let y:int = 2;
let z:any = "text";
```

### 3.2 Function

```gs
fn f(a, b = 10, ...rest) {
    return a;
}
```

### 3.3 Class

```gs
class C extends Base {
    x:int = 1;

    fn __new__(self) {
        return self;
    }
}
```

## 4. Statements

- `let ...;`
- expression statement: `expr;`
- `if / elif / else`
- `while`
- `for (i in range(a, b))`
- `for (v in listOrTuple)`
- `for (k, v in dict)`
- `break;`, `continue;`
- `return expr;`
- `throw expr;` and `throw;` (rethrow)
- `try { } catch (...) { } finally { }`
- `sleep <int_ms>;`
- `yield;`

## 5. Operators

Precedence is implemented in parser stages.

- Unary: `- ! ~`
- Power: `**` (right-associative)
- Multiplicative: `* / // %`
- Additive: `+ -`
- Shift: `<< >>`
- Membership: `in`, `not in`
- Comparison: `< <= > >=`
- Equality/identity: `== != is is not`
- Bitwise: `& ^ |`
- Logical: `&& ||`
- Assignment: `=` (variable/property/index targets)

## 6. Calls and Access

- Function call: `f(a, b)`
- Method call: `obj.m(a)`
- Property access: `obj.name`
- Index access: `obj[idx]`
- Index assignment: `obj[idx] = value`

## 7. Variadic and Unpack

- Parameter: `fn f(head, ...rest)`
- Call spread: `f(a, rest...)` where spread arg is last
- Declaration unpack:

```gs
let a, b:int = source...;
```

## 8. Lambda

```gs
let fn1 = (a, b) => {
    return a + b;
};
```

## 9. Imports

- `import module_name as alias;`
- `from module_name import symbol;`
- `from module_name import symbol as alias;`

## 10. Global Builtins

- `print(...)`
- `printf(format, args...)`
- `str(x)`
- `Tuple(...)`
- `type(x)`
- `typename(x)`
- `id(obj)`
- `assert(condition, format, args...)`
- `loadModule(name, [exports...])`
- `Exception([message])`

## 11. Built-in Modules

### 11.1 `system`

- `system.getTimeMs()`
- `system.gc([generation])`

### 11.2 `os` (selected)

- file: `open`, `read`, `write`, `append`, `remove`, `rename`
- path: `join`, `abspath`, `normalize`, `dirname`, `basename`, `extension`
- fs: `exists`, `isFile`, `isDirectory`, `fileSize`, `listdir`, `mkdir`
- env/cwd: `getcwd`, `chdir`, `sep`

### 11.3 `string`

- `string.format(fmt, args...)`
- `string.compile(pattern, [flags])`

## 12. Comments

- `# line comment`
- `/* block comment */`

`//` is operator `floor-div`, not a comment.

## 13. Runtime/Compiler Notes

- Type annotations are stored and enforced in compile/runtime paths depending on context.
- `spawn/await` are currently disabled by compiler even though tokens/statements exist.
- Bytecode serialization format is `GSBC3`.

## 14. Related Docs

- `LANGUAGE_TUTORIAL.md`
- `DESIGN_ARCHITECTURE.md`
- `features/PARAMETER_UNPACK_SPEC.md`
