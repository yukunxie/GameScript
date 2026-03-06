# GameScript Language Tutorial

This tutorial is practical and aligned with current repository behavior.

## 1. Run a Script

From project root, use the debug test runner (common in this repo):

```powershell
build\Debug\run_test.exe scripts\demo_simple.gs
```

You can replace script path with any tutorial file.

## 2. Program Entry

A common pattern is a `main` function returning status code:

```gs
fn main() {
    print("hello GameScript");
    return 0;
}
```

## 3. Variables and Basic Types

`let` defines variables.

```gs
let a = 10;
let b = 2.5;
let c = true;
let s = "text";
let n = null;
```

Type names can be inspected at runtime:

```gs
print(typename(a));
print(typename(b));
```

## 4. Operators

Supported operator groups include:

- Arithmetic: `+ - * / // % **`
- Comparison: `== != < <= > >=`
- Identity: `is`, `not is`
- Membership: `in`, `not in`
- Bitwise: `& | ^ ~ << >>`
- Logical: `&& || !`

Example:

```gs
let v = 2 + 3 * 4;
let f = 10 // 3;
let p = 2 ** 8;
let ok = (v > 10) && (3 in [1, 2, 3]);
```

## 5. Strings and Printing

```gs
print("A", 123, true);
printf("x=%d y=%.2f name=%s\n", 7, 3.14159, "gs");
let t = str(42);
```

## 6. Collections: List, Dict, Tuple

### 6.1 List

```gs
let list = [3, 1, 2];
list.push(9);
list.sort();
print(list.size());
print(list[0]);
```

### 6.2 Dict

```gs
let d = {};
d[1] = 100;
print(d[1]);
print(1 in d);
```

### 6.3 Tuple

```gs
let t = Tuple(10, 20, 30);
print(t.size());
print(t[1]);
```

## 7. Control Flow

### 7.1 If / Elif / Else

```gs
if (x > 10) {
    print("big");
} elif (x > 3) {
    print("mid");
} else {
    print("small");
}
```

### 7.2 While

```gs
let i = 0;
while (i < 5) {
    i = i + 1;
}
```

### 7.3 For Range

```gs
let sum = 0;
for (i in range(0, 10)) {
    sum = sum + i;
}
```

### 7.4 For List and Dict

```gs
for (v in [1,2,3]) {
    print(v);
}

for (k, v in {1:10, 2:20}) {
    print(k, v);
}
```

## 8. Functions

```gs
fn add(a, b) {
    return a + b;
}
```

Function defaults are supported:

```gs
fn sum3(a, b = 20, c = 30) {
    return a + b + c;
}
```

## 9. Variadic Parameters and Spread

Variadic parameter must be last:

```gs
fn collect(head, ...rest) {
    return head + rest.size();
}
```

Forward spread in calls:

```gs
fn call3(a, b, c) {
    return a + b + c;
}

fn forward(head, ...rest) {
    return call3(head, rest...);
}
```

Spread argument must be last in a call.

## 10. Multi-Target Unpack

```gs
fn pair(...rest) {
    let x, y:int = rest...;
    return x + y;
}
```

Rules:

- only valid with multi-variable `let`
- source must provide enough elements at runtime
- typed targets are checked

See detailed spec:

- `features/PARAMETER_UNPACK_SPEC.md`

## 11. Lambdas and Closures

Lambda syntax:

```gs
let mul = (x, y) => {
    return x * y;
};
```

Lambdas can capture outer variables.

## 12. Classes and Inheritance

```gs
class Animal {
    name = "animal";

    fn speak(self) {
        return 1;
    }
}

class Dog extends Animal {
    fn speak(self) {
        return super.speak(self);
    }
}
```

Common constructor style:

```gs
fn __new__(self, arg) {
    self.arg = arg;
    return self;
}
```

## 13. Exceptions

```gs
try {
    throw Exception("boom");
} catch (Exception as err) {
    print(err.message);
} finally {
    print("cleanup");
}
```

Typed catch example exists in:

- `scripts/test_exception_typed_catch.gs`

## 14. Type Utilities

Global helpers:

- `type(value)`
- `typename(value)`
- `id(obj)`

Example:

```gs
let t = type(1);
let v = t("42");
print(v, typename(v));
```

## 15. Imports and Modules

Supported styles:

```gs
import module_math as mm;
from module_math import add as plus;
```

Then use:

```gs
print(mm.add(1, 2));
print(plus(3, 4));
```

## 16. Built-in Modules

### 16.1 `system`

```gs
import system as system;
let now = system.getTimeMs();
let reclaimed = system.gc();
```

### 16.2 `os`

```gs
import os;
let cwd = os.getcwd();
let content = os.read("a.txt");
os.write("b.txt", content);
```

### 16.3 `string`

```gs
import string;
let s = string.format("value=%d", 7);
let p = string.compile("\\d+");
```

## 17. Comment Syntax

Use:

- line comment: `# ...`
- block comment: `/* ... */`

Note: `//` is floor-division operator, not a comment marker.

## 18. Current Limitations

- `spawn/await` are parsed but currently rejected by compiler (temporarily disabled)
- some module APIs are still evolving (for example `os.listdir` implementation notes in source)

## 19. Next Steps

- Read `LANGUAGE_REFERENCE.md` for concise syntax summary
- Read `DESIGN_ARCHITECTURE.md` for internals
- Run repository scripts under `scripts/` as executable examples
