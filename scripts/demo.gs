import object.creatures as creatures;
import misc.flow as flow;
import module_math as mm;
import module_globals as mg;
import system as system;
from module_math import add as plus;
from module_math import add, hello as gg;

let TOP_LEVEL_SENTINEL = 314;

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

fn test_loops_and_control_flow() {
    let rangeSum = 0;
    for (i in range(0, 10)) {
        let rangeSum = rangeSum + i;
    }
    assert(rangeSum == 45, "range sum expected 45, actual {}", rangeSum);

    let list = [1, 2, 3, 4];
    let listSum = 0;
    for (v in list) {
        let listSum = listSum + v;
    }
    assert(listSum == 10, "list sum expected 10, actual {}", listSum);

    let dict = {1: 7, 2: 8, 3: 9};
    let dictSum = 0;
    for (k, v in dict) {
        let dictSum = dictSum + v;
    }
    assert(dictSum == 24, "dict values sum expected 24, actual {}", dictSum);

    let i = 0;
    let whileSum = 0;
    while (i < 10) {
        let i = i + 1;
        if (i == 3) {
            continue;
        }
        if (i > 7) {
            break;
        }
        let whileSum = whileSum + i;
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

fn benchmark_hot_loop() {
    let total = 0;
    for (i in range(0, 10000)) {
        let total = total + i;
    }
    assert(total == 49995000, "hot loop checksum mismatch: {}", total);
    return total;
}

fn benchmark_module_calls() {
    let total = 0;
    for (i in range(0, 2000)) {
        let total = total + mm.add(i, 1);
    }
    assert(total == 2001000, "module-call benchmark checksum mismatch: {}", total);
    return total;
}

fn benchmark_printf() {
    let i = 100;
    let value = i * 7 + 3;
    printf("[printf-bench] i=%03d hex=%H str={} fp=%.2f\\n", i, value, value, value);
    let checksum = value;
    return checksum;
}

fn run_bench(verbose) {
    if (verbose == 1) {
        print("[bench] suite start");
        print("gg.hello", gg.hello(), TOP_LEVEL_SENTINEL);
    }

    let passed = 0;
    let passed = passed + test_arithmetic_and_string();
    let passed = passed + test_list_and_dict();
    let passed = passed + test_loops_and_control_flow();
    let passed = passed + test_modules_and_imports();
    let passed = passed + test_classes_and_host_objects();

    let checksum1 = benchmark_hot_loop();
    let checksum2 = benchmark_module_calls();
    let checksum3 = benchmark_printf();
    let reclaimed = system.gc();
    assert(reclaimed >= 0, "system.gc should be non-negative, actual {}", reclaimed);

    if (verbose == 1) {
        print("[bench] passed groups", passed);
        print("[bench] checksum1", checksum1);
        print("[bench] checksum2", checksum2);
        print("[bench] checksum3", checksum3);
        print("[bench] gc", reclaimed);
        print("[bench] suite done");
    }

    assert(passed == 5, "passed groups expected 5, actual {}", passed);
    return checksum1 + checksum2 + checksum3 + reclaimed;
}

fn ue_entry() {
    return run_bench(0);
}

fn main() {
    let total = run_bench(1);
    assert(total > 0, "bench total should be positive, actual {}", total);
    return 0;
}
