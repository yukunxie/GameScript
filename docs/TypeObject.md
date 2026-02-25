# TypeObject 特性文档

## 概述

`type()` 函数现在返回一个 **TypeObject**，而不是类型名称字符串。TypeObject 可以像函数一样被调用，用于进行类型转换。

## 基本用法

### 获取类型对象

```gs
let intType = type(1);           // <type 'Int'>
let floatType = type(3.14);      // <type 'Float'>
let boolType = type(true);       // <type 'Bool'>
let stringType = type("hello");  // <type 'String'>
let nullType = type(null);       // <type 'Null'>
```

### 使用类型对象进行转换

TypeObject 可以作为构造函数调用，接受一个参数并将其转换为目标类型：

```gs
let intType = type(1);

intType(42);        // 42 (Int)
intType(3.14);      // 3 (Float -> Int，截断)
intType(true);      // 1 (Bool -> Int)
intType(false);     // 0 (Bool -> Int)
intType("123");     // 123 (String -> Int，解析)
intType(null);      // 0 (Null -> Int)
```

## 支持的类型转换

### Int 类型转换

| 源类型   | 转换规则                    | 示例                    |
|----------|----------------------------|-------------------------|
| Int      | 原样返回                    | `Int(42)` → `42`        |
| Float    | 截断小数部分                | `Int(3.14)` → `3`       |
| Bool     | `true` → `1`, `false` → `0` | `Int(true)` → `1`       |
| String   | 解析数字字符串              | `Int("123")` → `123`    |
| Null     | 返回 `0`                    | `Int(null)` → `0`       |

### Float 类型转换

| 源类型   | 转换规则                | 示例                      |
|----------|------------------------|---------------------------|
| Float    | 原样返回                | `Float(3.14)` → `3.14`    |
| Int      | 转换为浮点数            | `Float(42)` → `42.0`      |
| Bool     | `true` → `1.0`, `false` → `0.0` | `Float(true)` → `1.0` |
| String   | 解析浮点数字符串        | `Float("3.14")` → `3.14`  |
| Null     | 返回 `0.0`              | `Float(null)` → `0.0`     |

### Bool 类型转换

| 源类型      | 转换规则                           | 示例                       |
|------------|-----------------------------------|----------------------------|
| Bool       | 原样返回                           | `Bool(true)` → `true`      |
| Int        | `0` → `false`, 其他 → `true`       | `Bool(1)` → `true`         |
| Float      | `0.0` → `false`, 其他 → `true`     | `Bool(3.14)` → `true`      |
| Null       | 返回 `false`                       | `Bool(null)` → `false`     |
| String/Ref | 始终返回 `true`                    | `Bool("hello")` → `true`   |

### String 类型转换

所有类型都可以转换为字符串，使用其默认的字符串表示：

```gs
let stringType = type("hello");

stringType(42);     // "42"
stringType(3.14);   // "3.14"
stringType(true);   // "true"
stringType(null);   // "null"
```

## 实际应用示例

### 1. 动态类型转换

```gs
fn convertToInt(value) {
    let IntType = type(0);
    return IntType(value);
}

let result = convertToInt("100");  // 100
```

### 2. 类型验证与转换

```gs
let intType = type(1);
let floatType = type(3.14);

let value = "42";
if (type(value) == type("")) {
    value = intType(value);  // 将字符串转换为 Int
}
```

### 3. 批量转换

```gs
let intType = type(0);
let values = ["1", "2", "3"];

// 在支持 map 的情况下：
// let converted = values.map((v) => intType(v));
```

## 类型名称映射

内部类型名会自动映射到标准名称：

| 内部名称 | 标准名称 |
|----------|----------|
| `int`    | `Int`    |
| `float`  | `Float`  |
| `bool`   | `Bool`   |
| `string` | `String` |
| `null`   | `Null`   |

## 错误处理

如果转换失败（例如，无法将非数字字符串转换为 Int），会抛出运行时错误：

```gs
let intType = type(0);
intType("abc");  // 抛出错误: Cannot convert string to Int: abc
```

## 实现细节

- **TypeObject** 是一个可调用对象，实现了类型系统的 `Object` 接口
- 调用 TypeObject 时，会触发 VM 中的 `CallValue` 操作码
- 类型转换逻辑在 `TypeObject::convert()` 方法中实现
- 支持通过 VM 的动态调度机制调用

## 测试

运行完整的类型对象测试：

```bash
build\Release\run_test.exe scripts\test_type_full.gs
```

## 未来扩展

可能的增强方向：

1. 支持自定义类型的 TypeObject
2. 添加类型检查方法（如 `intType.check(value)`）
3. 支持更复杂的转换规则（如安全转换 vs 强制转换）
4. 添加类型元信息（如大小、范围等）
