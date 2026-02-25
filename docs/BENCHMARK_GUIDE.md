# GameScript Benchmark Guide

## Overview

The GameScript benchmark system provides comprehensive performance testing integrated into `demo.gs`. It measures execution speed across various language features and operations.

## Quick Start

### Run All Tests and Benchmarks

```bash
cmake --build build --config Release
./build/Release/gamescript_app
```

### Expected Output

```
================================================================================
              GameScript Demo - Functional Tests & Benchmarks
================================================================================

[1/2] Running Functional Tests...
[bench] suite start
...
[bench] All functional tests passed!

[2/2] Running Performance Benchmarks...
================================================================================
                      GameScript Performance Benchmark
================================================================================

  Hot Loop (10K iterations)                     XX.XX ms/op  (10 iter)
  Module Calls (2K calls)                       XX.XX ms/op  (10 iter)
  Iterator Traversal (List/Tuple/Dict)          XX.XX ms/op  (10 iter)
  Vec2 Operations (1K ops)                      XX.XX ms/op  (10 iter)
  Entity Creation (100 entities)                XX.XX ms/op  (10 iter)
  String Operations (500 ops)                   XX.XX ms/op  (10 iter)
  Lambda Creation (1K lambdas)                  XX.XX ms/op  (10 iter)
  Operators (bitwise/logical/etc)               XX.XX ms/op  (10 iter)
  Lambda Closures (complex)                     XX.XX ms/op  (10 iter)
  Printf Operations                             XX.XX ms/op  (10 iter)

--------------------------------------------------------------------------------
  Total time: XXX.XX ms
  Total operations: 100
  Average time per test: XX.XX ms
================================================================================

Garbage collection reclaimed: XXXX bytes

[script] Demo completed successfully!
```

## Benchmark Framework

### BenchmarkSuite Class

The benchmark system is implemented as a reusable class in GameScript:

```javascript
class BenchmarkSuite {
    fn init() {
        this.tests = [];
        this.results = [];
    }

    fn add(name, testFn) {
        this.tests.push({
            "name": name,
            "fn": testFn
        });
    }

    fn run() {
        // Runs all tests with warm-up and timing
        // Returns results array
    }
}
```

### Usage Example

```javascript
let suite = BenchmarkSuite();
suite.init();

suite.add("My Benchmark", my_test_function);
suite.add("Another Test", another_function);

let results = suite.run();
```

## Benchmark Tests

### 1. Hot Loop (10K iterations)
Tests raw loop performance with simple arithmetic.

```javascript
fn benchmark_hot_loop() {
    let total = 0;
    for (i in range(0, 10000)) {
        total = total + i;
    }
    return total;
}
```

**Measures**: Loop overhead, integer arithmetic, variable access

### 2. Module Calls (2K calls)
Tests function call overhead across module boundaries.

```javascript
fn benchmark_module_calls() {
    let total = 0;
    for (i in range(0, 2000)) {
        total = total + mm.add(i, 1);
    }
    return total;
}
```

**Measures**: Module lookup, function dispatch, parameter passing

### 3. Iterator Traversal
Tests iteration over different container types.

```javascript
fn benchmark_iter_traversal() {
    // Creates and iterates over List, Tuple, Dict
    let list = [...];  // 1000 elements
    let tuple = Tuple(...);
    let dict = {...};
    
    // Sum all elements
    for (element in list) { ... }
    for (element in tuple) { ... }
    for (k, v in dict) { ... }
    
    return checksum;
}
```

**Measures**: List/Tuple/Dict iteration, subscript access

### 4. Vec2 Operations (1K ops)
Tests native C++ object binding performance.

```javascript
fn benchmark_vec2_operations() {
    let total = 0;
    for (i in range(0, 1000)) {
        let v1 = Vec2(i, i + 1);
        let v2 = Vec2(i + 2, i + 3);
        let v3 = v1.add(v2);
        total = total + v3.x + v3.y;
        let len = v3.length();
        total = total + len;
    }
    return total;
}
```

**Measures**: Native object creation, method calls, field access

### 5. Entity Creation (100 entities)
Tests complex object lifecycle management.

```javascript
fn benchmark_entity_creation() {
    let entities = [];
    for (i in range(0, 100)) {
        let e = Entity();
        e.HP = 100 + i;
        e.MP = 50 + i;
        e.Speed = 5 + i;
        e.Position = Vec2(i, i * 2);
        entities.push(e);
    }
    return total_hp;
}
```

**Measures**: Object allocation, property access, nested objects

### 6. String Operations (500 ops)
Tests string creation and type checking.

```javascript
fn benchmark_string_operations() {
    let total = 0;
    for (i in range(0, 500)) {
        let s = str(i);
        total = total + type(s) == "string";
    }
    return total;
}
```

**Measures**: String conversion, type introspection

### 7. Lambda Creation (1K lambdas)
Tests lambda/closure creation overhead.

```javascript
fn benchmark_lambda_creation() {
    let total = 0;
    for (i in range(0, 1000)) {
        let multiplier = (x) => {
            return x * i;
        };
        total = total + multiplier(2);
    }
    return total;
}
```

**Measures**: Closure allocation, capture overhead, lambda invocation

### 8. Operators (bitwise/logical)
Tests all operator types and precedence.

```javascript
fn benchmark_operators() {
    // Tests: //, %, **, &, |, ^, ~, <<, >>, &&, ||, is, in, not in
    let loopSum = 0;
    for (i in range(0, 1000)) {
        loopSum = loopSum + (i % 10) + (i // 10) + ((i & 7) | (i ^ 3));
    }
    return checksum;
}
```

**Measures**: Bitwise ops, logical ops, operator precedence

### 9. Lambda Closures (complex)
Tests complex closure semantics and capture.

```javascript
fn benchmark_lambda_closures() {
    let base = 10;
    let adder = (x, y) => {
        let sum = x + y;
        base = base + sum;  // Captures and modifies
        return base;
    };
    
    // Multiple calls with state mutation
    for (i in range(0, 1000)) {
        acc = acc + adder(i, 1);
    }
    
    return checksum;
}
```

**Measures**: Closure state management, capture-by-reference

### 10. Printf Operations
Tests formatted output performance.

```javascript
fn benchmark_printf() {
    let i = 100;
    let value = i * 7 + 3;
    // Tests format string parsing and substitution
    return checksum;
}
```

**Measures**: String formatting, vararg handling

## Performance Metrics

### What Gets Measured

1. **Execution Time**: Wallclock time per operation
2. **Iterations**: Number of runs (10 per test with warm-up)
3. **Checksum**: Verification that results are correct
4. **Memory**: GC reclaimed bytes after all tests

### Benchmark Process

For each test:
1. **Warm-up**: Run once to eliminate cold-start effects
2. **Timing**: Run 10 times with `system.getTimeMs()` calls
3. **Average**: Calculate mean time per operation
4. **Verify**: Assert checksum matches expected value

## Adding New Benchmarks

### Step 1: Write Test Function

```javascript
fn my_new_benchmark() {
    let result = 0;
    // Your benchmark code here
    for (i in range(0, 1000)) {
        result = result + compute(i);
    }
    return result;  // Return checksum for verification
}
```

### Step 2: Add to Suite

```javascript
fn run_performance_benchmark() {
    let suite = BenchmarkSuite();
    suite.init();
    
    // ... existing tests ...
    suite.add("My New Test Description", my_new_benchmark);
    
    let results = suite.run();
    return results;
}
```

### Best Practices

1. **Return Checksum**: Always return a verifiable result
2. **Consistent Scale**: Use similar iteration counts for comparison
3. **Warm-up**: First run is discarded automatically
4. **No I/O**: Avoid `print()` inside tight loops
5. **Deterministic**: Results should be reproducible

## Interpreting Results

### Typical Performance Ranges

*(Example on modern x64 CPU, Release build)*

| Benchmark                  | Expected Range | What It Means               |
|----------------------------|----------------|-----------------------------|
| Hot Loop                   | 0.1-1 ms/op    | Core VM speed              |
| Module Calls               | 0.5-2 ms/op    | Function dispatch cost     |
| Iterator Traversal         | 1-5 ms/op      | Container iteration        |
| Vec2 Operations            | 5-15 ms/op     | Native binding overhead    |
| Entity Creation            | 10-30 ms/op    | Object allocation cost     |
| Lambda Creation            | 5-15 ms/op     | Closure creation cost      |

### Performance Flags

- **Debug vs Release**: Expect 3-10x difference
- **Optimization**: `-O3` vs `-O0` significantly impacts results
- **Platform**: x64 vs ARM, Windows vs Linux variations

## Continuous Integration

### Regression Detection

Compare results against baseline:

```bash
# Run benchmark and save results
./gamescript_app > benchmark_results.txt

# Compare against baseline
diff benchmark_results.txt baseline/benchmark_results.txt
```

### Performance Goals

- **No Regression**: ±5% variance acceptable
- **Red Flag**: >20% slowdown requires investigation
- **Improvement**: >10% speedup should be documented

## Troubleshooting

### Inconsistent Results

**Problem**: Results vary wildly between runs

**Solutions**:
- Close other applications
- Run on dedicated core (CPU affinity)
- Increase iteration count
- Check thermal throttling

### Unexpectedly Slow

**Problem**: Much slower than expected

**Solutions**:
- Verify Release build (`-O3`)
- Check compiler version
- Profile with perf/VTune
- Validate no debug logging

### Memory Issues

**Problem**: GC reclaims huge amounts

**Solutions**:
- Check for leaks in benchmark code
- Verify object cleanup
- Increase GC threshold
- Profile memory allocations

## Advanced Usage

### Custom Iterations

Modify iteration count per test:

```javascript
// In BenchmarkSuite.run()
let iterations = 10;  // Change this value
```

### Filtering Tests

Run specific benchmarks only:

```javascript
suite.add("Hot Loop", benchmark_hot_loop);
// Comment out tests you don't want to run
// suite.add("Module Calls", benchmark_module_calls);
```

### Export Results

Save results to file (requires C++ integration):

```javascript
fn export_results(results) {
    for (result in results) {
        printf("%s,%.2f\\n", result["name"], result["avgTime"]);
    }
}
```

## See Also

- **BINDING_API.md** - Native object binding performance
- **PROJECT_STRUCTURE.md** - Code organization
- **demo.gs** - Full benchmark source code

---

**Last Updated**: 2026-02-25  
**Version**: 1.0.0
