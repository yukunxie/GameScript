import object.creatures as creatures;
import misc.flow as flow;
import module_math as mm;
import module_globals as mg;
import system as system;
from module_math import add as plus;
from module_math import add, hello as gg;

let TOP_LEVEL_SENTINEL = 314;
let LAMBDA_TOP_SCALE = 3;
let TOP_MUL = (x) => {
    return x * LAMBDA_TOP_SCALE;
};

fn test_load_const()
{
    let k = 2;
    k = k + 2 + 2 - 3 * 4 / 9;
    print ("test_load_const k =", k);
    # assert(k == 5.555555555555555, "k expected 5.555555555555555, actual {}", k);
}

fn test_arithmetic_and_string() {
    let x = 40 + 2;
    assert(x == 42, "x expected 42, actual {}", x);

    let y = x * 2 - 20;
    assert(y == 64, "y expected 64, actual {}", y);

    let f = 1.25;
    assert(type(f) == "float", "f type expected float, actual {}", type(f));
    let mix = f + 2;
    assert(type(mix) == "float", "mix type expected float, actual {}", type(mix));
    assert(mix == 3.25, "mix expected 3.25, actual {}", mix);

    print ("f-mix", f, mix, type(f), type(mix));

    let quotient = 5 / 2;
    assert(type(quotient) == "float", "quotient type expected float, actual {}", type(quotient));
    assert(quotient == 2.5, "quotient expected 2.5, actual {}", quotient);

    let s = str(y);
    assert(s == "64", "str(y) expected 64, actual {}", s);
    return 1;
}

fn test_list_and_dict() {
    let list = [];
    list.push(10);
    list.push(20);
    list.push(5);
    list.sort();

    assert(list.size() == 3, "list size expected 3, actual {}", list.size());
    assert(list[0] == 5, "list[0] expected 5, actual {}", list[0]);

    let removed = list.remove(1);
    assert(removed == 10, "removed expected 10, actual {}", removed);
    assert(list.size() == 2, "list size expected 2, actual {}", list.size());

    let dict = {};
    dict[1] = 100;
    dict[2] = 200;
    assert(dict[1] == 100, "dict[1] expected 100, actual {}", dict[1]);
    let deleted = dict.del(2);
    assert(deleted == 200, "dict.del(2) expected 200, actual {}", deleted);
    assert(dict.size() == 1, "dict size expected 1, actual {}", dict.size());
    return 1;
}

fn test_tuple() {
    let t = Tuple(10, 20, 30);
    assert(type(t) == "Tuple", "tuple type expected Tuple, actual {}", type(t));
    assert(t.size() == 3, "tuple size expected 3, actual {}", t.size());
    assert(t[1] == 20, "tuple[1] expected 20, actual {}", t[1]);

    t[1] = 99;
    assert(t[1] == 99, "tuple[1] expected 99 after set, actual {}", t[1]);
    assert(t.size() == 3, "tuple size must stay 3, actual {}", t.size());
    return 1;
}

fn test_loops_and_control_flow() {
    let rangeSum = 0;
    for (i in range(0, 10)) {
        rangeSum = rangeSum + i;
    }
    assert(rangeSum == 45, "range sum expected 45, actual {}", rangeSum);

    let list = [1, 2, 3, 4];
    let listSum = 0;
    for (v in list) {
        listSum = listSum + v;
    }
    assert(listSum == 10, "list sum expected 10, actual {}", listSum);

    let tuple = Tuple(5, 6, 7, 8);
    let tupleSum = 0;
    for (element in tuple) {
        tupleSum = tupleSum + element;
    }
    assert(tupleSum == 26, "tuple sum expected 26, actual {}", tupleSum);

    let dict = {1: 7, 2: 8, 3: 9};
    let dictSum = 0;
    for (k, v in dict) {
        dictSum = dictSum + v;
    }
    assert(dictSum == 24, "dict values sum expected 24, actual {}", dictSum);

    let whileIndex = 0;
    let whileSum = 0;
    while (whileIndex < 10) {
        whileIndex = whileIndex + 1;
        if (whileIndex == 3) {
            continue;
        }
        if (whileIndex > 7) {
            break;
        }
        whileSum = whileSum + whileIndex;
    }
    assert(whileSum == 25, "while sum expected 25, actual {}", whileSum);
    return 1;
}

fn test_modules_and_imports() {
    assert(TOP_LEVEL_SENTINEL == 314, "top-level sentinel mismatch: {}", TOP_LEVEL_SENTINEL);
    assert(mg.G == 123, "module global G expected 123, actual {}", mg.G);
    assert(mg.getG() == 123, "mg.getG expected 123, actual {}", mg.getG());
    assert(mg.getDouble() == 246, "mg.getDouble expected 246, actual {}", mg.getDouble());

    assert(mm.add(2, 3) == 5, "mm.add expected 5, actual {}", mm.add(2, 3));
    assert(plus(4, 5) == 9, "plus expected 9, actual {}", plus(4, 5));
    assert(flow.loop_demo() == 10, "flow.loop_demo expected 10, actual {}", flow.loop_demo());
    return 1;
}

fn test_classes_and_host_objects() {
    let animal = creatures.Animal();
    let dog = creatures.Dog();
    assert(type(animal) == "Animal", "animal type expected Animal, actual {}", type(animal));
    assert(type(dog) == "Dog", "dog type expected Dog, actual {}", type(dog));

    assert(animal.speak() == 1, "animal.speak expected 1, actual {}", animal.speak());
    dog.VoiceFn = creatures.animalVoice;
    assert(dog.speak() == 1, "dog.speak expected 1 after override, actual {}", dog.speak());

    let p = Vec2(9, 8);
    assert(p.x == 9, "Vec2.x expected 9, actual {}", p.x);
    p.y = 23;
    assert(p.y == 23, "Vec2.y expected 23, actual {}", p.y);

    let e = Entity();
    assert(e.HP == 100, "Entity.HP expected 100, actual {}", e.HP);
    e.HP = 135;
    e.MP = 88;
    e.Speed = 9;
    e.Position = p;
    e.GotoPoint(p);
    assert(e.HP == 135, "Entity.HP expected 135, actual {}", e.HP);
    assert(type(id(e)) == "int", "type(id(e)) expected int, actual {}", type(id(e)));
    return 1;
}

// ============================================================================
// Benchmark Framework
// ============================================================================

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
        print("================================================================================");
        print("                      GameScript Performance Benchmark");
        print("================================================================================");
        print("");

        let totalTime = 0;
        let totalOps = 0;

        for (test in this.tests) {
            let name = test["name"];
            let fn = test["fn"];
            
            // Warm up
            fn();
            
            // Run benchmark
            let startTime = system.getTimeMs();
            let iterations = 10;
            let checksum = 0;
            
            for (i in range(0, iterations)) {
                checksum = checksum + fn();
            }
            
            let endTime = system.getTimeMs();
            let elapsed = endTime - startTime;
            let avgTime = elapsed / iterations;
            
            this.results.push({
                "name": name,
                "avgTime": avgTime,
                "iterations": iterations,
                "checksum": checksum
            });
            
            totalTime = totalTime + elapsed;
            totalOps = totalOps + iterations;
            
            printf("  %-40s %8.2f ms/op  (%d iter)\\n", name, avgTime, iterations);
        }

        print("");
        print("--------------------------------------------------------------------------------");
        printf("  Total time: %.2f ms\\n", totalTime);
        printf("  Total operations: %d\\n", totalOps);
        printf("  Average time per test: %.2f ms\\n", totalTime / this.tests.size());
        print("================================================================================");
        print("");
        
        return this.results;
    }
}

// ============================================================================
// Individual Benchmark Tests
// ============================================================================

fn benchmark_hot_loop() {
    let total = 0;
    for (i in range(0, 10000)) {
        total = total + i;
    }
    assert(total == 49995000, "hot loop checksum mismatch: {}", total);
    return total;
}

fn benchmark_module_calls() {
    let total = 0;
    for (i in range(0, 2000)) {
        total = total + mm.add(i, 1);
    }
    assert(total == 2001000, "module-call benchmark checksum mismatch: {}", total);
    return total;
}

fn benchmark_iter_traversal() {
    let list = [];
    for (i in range(0, 1000)) {
        list.push(i);
    }

    let tuple = Tuple(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);

    let dict = {};
    for (i in range(0, 1000)) {
        dict[i] = i + 1;
    }

    let sumList = 0;
    for (element in list) {
        sumList = sumList + element;
    }

    let sumTuple = 0;
    for (element in tuple) {
        sumTuple = sumTuple + element;
    }

    let sumDict = 0;
    for (k, v in dict) {
        sumDict = sumDict + k + v;
    }

    let checksum = sumList + sumTuple + sumDict;
    assert(checksum == 1499555, "iter-traversal benchmark checksum mismatch: {}", checksum);
    return checksum;
}

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
    
    let totalHP = 0;
    for (e in entities) {
        totalHP = totalHP + e.HP;
    }
    
    return totalHP;
}

fn benchmark_string_operations() {
    let total = 0;
    for (i in range(0, 500)) {
        let s = str(i);
        total = total + type(s) == "string";
    }
    return total;
}

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

fn benchmark_operators() {
    # Arithmetic operators: floor division, modulo, power
    let a = 17;
    let b = 5;
    let floorDiv = a // b;
    assert(floorDiv == 3, "floor division expected 3, actual {}", floorDiv);
    
    let modResult = a % b;
    assert(modResult == 2, "modulo expected 2, actual {}", modResult);
    
    let powerResult = 2 ** 10;
    assert(powerResult == 1024, "power expected 1024, actual {}", powerResult);
    
    # Bitwise operators
    let x = 12;  # 0b1100
    let y = 5;   # 0b0101
    
    let andResult = x & y;
    assert(andResult == 4, "bitwise AND expected 4, actual {}", andResult);
    
    let orResult = x | y;
    assert(orResult == 13, "bitwise OR expected 13, actual {}", orResult);
    
    let xorResult = x ^ y;
    assert(xorResult == 9, "bitwise XOR expected 9, actual {}", xorResult);
    
    let notResult = ~5;
    assert(notResult == -6, "bitwise NOT expected -6, actual {}", notResult);
    
    let leftShift = 3 << 2;
    assert(leftShift == 12, "left shift expected 12, actual {}", leftShift);
    
    let rightShift = 16 >> 2;
    assert(rightShift == 4, "right shift expected 4, actual {}", rightShift);
    
    # Logical operators
    let logicalAnd = (5 > 3) && (10 < 20);
    assert(logicalAnd == 1, "logical AND expected 1, actual {}", logicalAnd);
    
    let logicalOr = (5 < 3) || (10 < 20);
    assert(logicalOr == 1, "logical OR expected 1, actual {}", logicalOr);
    
    # Identity operators
    let num1 = 42;
    let num2 = 42;
    let isIdentical = num1 is num2;
    assert(isIdentical == 1, "identity check expected 1, actual {}", isIdentical);
    
    # Membership operators
    let list = [1, 2, 3, 4, 5];
    let inList = 3 in list;
    assert(inList == 1, "membership check expected 1, actual {}", inList);
    
    let notInList = 10 not in list;
    assert(notInList == 1, "not in membership expected 1, actual {}", notInList);
    
    let dict = {1: 10, 2: 20, 3: 30};
    let inDict = 2 in dict;
    assert(inDict == 1, "dict membership expected 1, actual {}", inDict);
    
    let tuple = Tuple(7, 8, 9);
    let inTuple = 8 in tuple;
    assert(inTuple == 1, "tuple membership expected 1, actual {}", inTuple);
    
    # Operator precedence test
    let precedence = 2 + 3 ** 2;
    assert(precedence == 11, "precedence expected 11, actual {}", precedence);
    
    let complexExpr = 10 + 3 * 2 ** 2 - 4 / 2;
    assert(complexExpr == 20, "complex expression expected 20, actual {}", complexExpr);
    
    # Performance test: heavy operator loop
    let loopSum = 0;
    for (i in range(0, 1000)) {
        loopSum = loopSum + (i % 10) + (i // 10) + ((i & 7) | (i ^ 3));
    }
    assert(loopSum == 555000, "operator loop checksum expected 555000, actual {}", loopSum);
    
    let checksum = floorDiv + modResult + powerResult + andResult + orResult 
                 + xorResult + leftShift + rightShift + logicalAnd + logicalOr 
                 + inList + notInList + inDict + inTuple + precedence 
                 + complexExpr + loopSum + notResult;
    assert(checksum == 556102, "operator benchmark checksum expected 556102, actual {}", checksum);
    
    return checksum;
}

fn benchmark_lambda_closures_return()
{
    let base = 10;
    let adder = (x, y) => {
        let sum = x + y;
        let preBase = base;
        base = base + sum;
        return base;
    };
    return adder;
}

fn benchmark_lambda_closures() {
    let base = 10;
    let adder = (x, y) => {
        let sum = x + y;
        base = base + sum;
        return base;
    };

    let r1 = adder(1, 2);
    let r2 = adder(3, 4);
    assert(r1 == 13, "lambda r1 expected 13, actual {}", r1);
    assert(r2 == 20, "lambda r2 expected 20, actual {}", r2);

    let shadow = (x) => {
        let base = x + 1;
        return base;
    };
    assert(shadow(5) == 6, "lambda shadow expected 6, actual {}", shadow(5));

    let topR = TOP_MUL(7);
    assert(topR == 21, "TOP_MUL expected 21, actual {}", topR);

    let acc = 0;
    for (i in range(0, 1000)) {
        acc = acc + adder(i, 1);
    }

    assert(base == 500520, "lambda captured base expected 500520, actual {}", base);
    assert(acc == 167187000, "lambda loop acc expected 167187000, actual {}", acc);

    let checksum = r1 + r2 + shadow(9) + topR + acc + base + TOP_MUL(100);
    assert(checksum == 167687884, "lambda benchmark checksum mismatch: {}", checksum);
    return checksum;
}

fn benchmark_printf() {
    let i = 100;
    let value = i * 7 + 3;
    let checksum = value;
    return checksum;
}

// ============================================================================
// Run Performance Benchmark Suite
// ============================================================================

fn run_performance_benchmark() {
    let suite = BenchmarkSuite();
    suite.init();
    
    suite.add("Hot Loop (10K iterations)", benchmark_hot_loop);
    suite.add("Module Calls (2K calls)", benchmark_module_calls);
    suite.add("Iterator Traversal (List/Tuple/Dict)", benchmark_iter_traversal);
    suite.add("Vec2 Operations (1K ops)", benchmark_vec2_operations);
    suite.add("Entity Creation (100 entities)", benchmark_entity_creation);
    suite.add("String Operations (500 ops)", benchmark_string_operations);
    suite.add("Lambda Creation (1K lambdas)", benchmark_lambda_creation);
    suite.add("Operators (bitwise/logical/etc)", benchmark_operators);
    suite.add("Lambda Closures (complex)", benchmark_lambda_closures);
    suite.add("Printf Operations", benchmark_printf);
    
    let results = suite.run();
    
    # GC test
    let reclaimed = system.gc();
    print("Garbage collection reclaimed:", reclaimed, "bytes");
    print("");
    
    return results;
}

// ============================================================================
// Run Tests
// ============================================================================

fn run_bench(verbose) {
    if (verbose == 1) {
        print("[bench] suite start");
        print("gg.hello", gg.hello(), TOP_LEVEL_SENTINEL);
    }

    test_load_const();

    let passed = 0;
    passed = passed + test_arithmetic_and_string();
    passed = passed + test_list_and_dict();
    passed = passed + test_tuple();
    passed = passed + test_loops_and_control_flow();
    passed = passed + test_modules_and_imports();
    passed = passed + test_classes_and_host_objects();

    let checksum1 = benchmark_hot_loop();
    let checksum2 = benchmark_module_calls();
    let checksum3 = benchmark_iter_traversal();
    let checksum4 = benchmark_printf();
    let checksum5 = benchmark_lambda_closures();
    let checksum6 = benchmark_operators();
    let reclaimed = system.gc();
    assert(reclaimed >= 0, "system.gc should be non-negative, actual {}", reclaimed);

    let adder = benchmark_lambda_closures_return();
    let r = adder(10, 20);
    if (verbose == 1) {
        print ("adder(10, 20) returned", r);
    }
    assert(r == 30 + 10, "closure returned from benchmark_lambda_closures_return expected to capture base 10, actual {}", r);

    if (verbose == 1) {
        print("[bench] passed groups", passed);
        print("[bench] checksum1", checksum1);
        print("[bench] checksum2", checksum2);
        print("[bench] checksum3", checksum3);
        print("[bench] checksum4", checksum4);
        print("[bench] checksum5", checksum5);
        print("[bench] checksum6 (operators)", checksum6);
        print("[bench] gc", reclaimed);
        print("[bench] suite done");
    }

    assert(passed == 6, "passed groups expected 6, actual {}", passed);
    return checksum1 + checksum2 + checksum3 + checksum4 + checksum5 + checksum6 + reclaimed;
}

fn ue_entry() {
    return run_bench(0);
}

fn main() {
    print("");
    print("================================================================================");
    print("              GameScript Demo - Functional Tests & Benchmarks");
    print("================================================================================");
    print("");
    
    # Run functional tests first
    print("[1/2] Running Functional Tests...");
    let total = run_bench(1);
    assert(total > 0, "bench total should be positive, actual {}", total);
    print("[script] All functional tests passed!");
    print("");
    
    # Run performance benchmarks
    print("[2/2] Running Performance Benchmarks...");
    let results = run_performance_benchmark();
    
    print("[script] Demo completed successfully!");
    return 0;
}
