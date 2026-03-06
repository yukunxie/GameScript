# Parameter Unpack and Variadic Forwarding Specification

## Scope

This document defines the language behavior for:
- Default parameters
- Variadic parameters
- Variadic forwarding with `expr...` in call arguments
- Multi-target unpack declarations with `let a, b = expr...`

## Function Parameters

### Default Parameters

Syntax:

```gs
fn f(a, b = 10, c = 20) { ... }
```

Rules:
- A non-default parameter cannot appear after a default parameter.
- Default parameter expressions must be compile-time constants or side-effect-free arithmetic expressions.

### Variadic Parameters

Syntax:

```gs
fn f(head, ...rest) { ... }
```

Rules:
- Variadic parameter must be the last parameter.
- Variadic parameter cannot have a default value.
- Runtime binding packs extra arguments into `rest` as a list-like value.

## Variadic Forwarding

Syntax:

```gs
fn f(head, ...rest) {
    return g(head, rest...);
}
```

Semantics:
- `rest...` expands one source value into positional call arguments.
- The unpack argument must be the last argument in the call.
- The spread source must be an object that provides `size()` and `get(index)` behavior at runtime.

Applicable call forms:
- Value call: `callee(...args)`
- Method call: `obj.method(...args)`

## Multi-Target Unpack Declaration

Syntax:

```gs
let a, b:int = source...;
```

Rules:
- Multi-target declaration requires unpack source syntax (`expr...`).
- Unpack source is evaluated once.
- Runtime checks enforce `source.size() >= target_count`.
- If size is insufficient, runtime throws an exception.

Assignment semantics:
- `target[i] = source.get(i)` for each declared target.
- Type annotations follow existing declaration rules.

Type checking:
- If target has no type annotation (or `any`), any value is accepted.
- If target has a type annotation, normal assignment type checks apply.
- In debug builds, runtime type checks are enforced.
- Compile-time type check is attempted for statically-known list literals.

## Error Conditions

Parser errors:
- Variadic parameter not in last position
- Variadic parameter with default value
- Non-default parameter after default parameter
- Unpack argument not in last call position
- Multi-target `let` without unpack source

Compile-time errors:
- Constant list literal unpack with fewer elements than declared targets
- Constant list literal unpack with known element type mismatch against declared target type

Runtime errors:
- Spread source is not unpackable
- Spread source does not provide valid `size()` / `get(index)` semantics
- Unpack declaration source size is smaller than declared target count
- Debug-only type mismatch on typed targets
