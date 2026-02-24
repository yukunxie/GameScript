# Demo GS vs Python 指令数对比

- GS 字节码来源: `D:/GameScript/scripts/.gsdebug/demo.opcode.dis`
- GS 源码: `D:/GameScript/scripts/demo.gs`
- Python 源码: `D:/GameScript/scripts/demo.py`

## 汇总

| 指标 | 数值 |
|---|---:|
| GS 指令总数 | 1102 |
| Python 指令总数 | 1044 |
| 差值 (Py - GS) | -58 |
| 比例 (Py / GS) | 0.9474 |

## 函数级对比

| 函数 | GS 指令数 | Py 指令数 | 差值(Py-GS) |
|---|---:|---:|---:|
| `test_load_const` | 22 | 18 | -4 |
| `test_arithmetic_and_string` | 118 | 110 | -8 |
| `test_list_and_dict` | 119 | 122 | 3 |
| `test_tuple` | 79 | 73 | -6 |
| `test_loops_and_control_flow` | 164 | 147 | -17 |
| `test_modules_and_imports` | 92 | 87 | -5 |
| `test_classes_and_host_objects` | 163 | 143 | -20 |
| `benchmark_hot_loop` | 23 | 26 | 3 |
| `benchmark_module_calls` | 28 | 31 | 3 |
| `benchmark_iter_traversal` | 130 | 104 | -26 |
| `benchmark_printf` | 19 | 30 | 11 |
| `run_bench` | 127 | 133 | 6 |
| `ue_entry` | 4 | 5 | 1 |
| `main` | 14 | 15 | 1 |

## 左右对比（含函数原代码）

### `test_load_const`

- GS 指令数: **22**  |  Py 指令数: **18**  |  差值(Py-GS): **-4**

<table>
  <tr>
    <th align="left" width="50%">GameScript (GS)</th>
    <th align="left" width="50%">Python (Py)</th>
  </tr>
  <tr>
    <td valign="top">

```gs
fn test_load_const()
{
    let k = 2;
    k = k + 2 + 2 - 3 * 4 / 9;
    print ("test_load_const k =", k);
    # assert(k == 5.555555555555555, "k expected 5.555555555555555, actual {}", k);
}
```

    </td>
    <td valign="top">

```python
def test_load_const():
    k = 2
    k = k + 2 + 2 - 3 * 4 / 9
    print("test_load_const k =", k)
```

    </td>
  </tr>
</table>

### `test_arithmetic_and_string`

- GS 指令数: **118**  |  Py 指令数: **110**  |  差值(Py-GS): **-8**

<table>
  <tr>
    <th align="left" width="50%">GameScript (GS)</th>
    <th align="left" width="50%">Python (Py)</th>
  </tr>
  <tr>
    <td valign="top">

```gs
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
```

    </td>
    <td valign="top">

```python
def test_arithmetic_and_string():
    x = 40 + 2
    gs_assert(x == 42, "x expected 42, actual {}", x)

    y = x * 2 - 20
    gs_assert(y == 64, "y expected 64, actual {}", y)

    f = 1.25
    gs_assert(gs_type(f) == "float", "f type expected float, actual {}", gs_type(f))
    mix = f + 2
    gs_assert(gs_type(mix) == "float", "mix type expected float, actual {}", gs_type(mix))
    gs_assert(mix == 3.25, "mix expected 3.25, actual {}", mix)

    print("f-mix", f, mix, gs_type(f), gs_type(mix))

    quotient = 5 / 2
    gs_assert(gs_type(quotient) == "float", "quotient type expected float, actual {}", gs_type(quotient))
    gs_assert(quotient == 2.5, "quotient expected 2.5, actual {}", quotient)

    s = str(y)
    gs_assert(s == "64", "str(y) expected 64, actual {}", s)
    return 1
```

    </td>
  </tr>
</table>

### `test_list_and_dict`

- GS 指令数: **119**  |  Py 指令数: **122**  |  差值(Py-GS): **3**

<table>
  <tr>
    <th align="left" width="50%">GameScript (GS)</th>
    <th align="left" width="50%">Python (Py)</th>
  </tr>
  <tr>
    <td valign="top">

```gs
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
```

    </td>
    <td valign="top">

```python
def test_list_and_dict():
    values = GSList()
    values.push(10)
    values.push(20)
    values.push(5)
    values.sort()

    gs_assert(values.size() == 3, "list size expected 3, actual {}", values.size())
    gs_assert(values[0] == 5, "list[0] expected 5, actual {}", values[0])

    removed = values.remove(1)
    gs_assert(removed == 10, "removed expected 10, actual {}", removed)
    gs_assert(values.size() == 2, "list size expected 2, actual {}", values.size())

    d = GSDict()
    d[1] = 100
    d[2] = 200
    gs_assert(d[1] == 100, "dict[1] expected 100, actual {}", d[1])
    deleted = d.del_(2)
    gs_assert(deleted == 200, "dict.del(2) expected 200, actual {}", deleted)
    gs_assert(d.size() == 1, "dict size expected 1, actual {}", d.size())
    return 1
```

    </td>
  </tr>
</table>

### `test_tuple`

- GS 指令数: **79**  |  Py 指令数: **73**  |  差值(Py-GS): **-6**

<table>
  <tr>
    <th align="left" width="50%">GameScript (GS)</th>
    <th align="left" width="50%">Python (Py)</th>
  </tr>
  <tr>
    <td valign="top">

```gs
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
```

    </td>
    <td valign="top">

```python
def test_tuple():
    t = GSTuple(10, 20, 30)
    gs_assert(gs_type(t) == "Tuple", "tuple type expected Tuple, actual {}", gs_type(t))
    gs_assert(t.size() == 3, "tuple size expected 3, actual {}", t.size())
    gs_assert(t[1] == 20, "tuple[1] expected 20, actual {}", t[1])

    t[1] = 99
    gs_assert(t[1] == 99, "tuple[1] expected 99 after set, actual {}", t[1])
    gs_assert(t.size() == 3, "tuple size must stay 3, actual {}", t.size())
    return 1
```

    </td>
  </tr>
</table>

### `test_loops_and_control_flow`

- GS 指令数: **164**  |  Py 指令数: **147**  |  差值(Py-GS): **-17**

<table>
  <tr>
    <th align="left" width="50%">GameScript (GS)</th>
    <th align="left" width="50%">Python (Py)</th>
  </tr>
  <tr>
    <td valign="top">

```gs
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
```

    </td>
    <td valign="top">

```python
def test_loops_and_control_flow():
    range_sum = 0
    for i in range(0, 10):
        range_sum += i
    gs_assert(range_sum == 45, "range sum expected 45, actual {}", range_sum)

    values = GSList([1, 2, 3, 4])
    list_sum = 0
    for v in values:
        list_sum += v
    gs_assert(list_sum == 10, "list sum expected 10, actual {}", list_sum)

    tup = GSTuple(5, 6, 7, 8)
    tuple_sum = 0
    for element in tup:
        tuple_sum += element
    gs_assert(tuple_sum == 26, "tuple sum expected 26, actual {}", tuple_sum)

    d = GSDict({1: 7, 2: 8, 3: 9})
    dict_sum = 0
    for _, value in d:
        dict_sum += value
    gs_assert(dict_sum == 24, "dict values sum expected 24, actual {}", dict_sum)

    while_index = 0
    while_sum = 0
    while while_index < 10:
        while_index += 1
        if while_index == 3:
            continue
        if while_index > 7:
            break
        while_sum += while_index

    gs_assert(while_sum == 25, "while sum expected 25, actual {}", while_sum)
    return 1
```

    </td>
  </tr>
</table>

### `test_modules_and_imports`

- GS 指令数: **92**  |  Py 指令数: **87**  |  差值(Py-GS): **-5**

<table>
  <tr>
    <th align="left" width="50%">GameScript (GS)</th>
    <th align="left" width="50%">Python (Py)</th>
  </tr>
  <tr>
    <td valign="top">

```gs
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
```

    </td>
    <td valign="top">

```python
def test_modules_and_imports():
    gs_assert(TOP_LEVEL_SENTINEL == 314, "top-level sentinel mismatch: {}", TOP_LEVEL_SENTINEL)
    gs_assert(mg.G == 123, "module global G expected 123, actual {}", mg.G)
    gs_assert(mg.getG() == 123, "mg.getG expected 123, actual {}", mg.getG())
    gs_assert(mg.getDouble() == 246, "mg.getDouble expected 246, actual {}", mg.getDouble())

    gs_assert(mm.add(2, 3) == 5, "mm.add expected 5, actual {}", mm.add(2, 3))
    gs_assert(plus(4, 5) == 9, "plus expected 9, actual {}", plus(4, 5))
    gs_assert(flow.loop_demo() == 10, "flow.loop_demo expected 10, actual {}", flow.loop_demo())
    return 1
```

    </td>
  </tr>
</table>

### `test_classes_and_host_objects`

- GS 指令数: **163**  |  Py 指令数: **143**  |  差值(Py-GS): **-20**

<table>
  <tr>
    <th align="left" width="50%">GameScript (GS)</th>
    <th align="left" width="50%">Python (Py)</th>
  </tr>
  <tr>
    <td valign="top">

```gs
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
```

    </td>
    <td valign="top">

```python
def test_classes_and_host_objects():
    animal = Animal()
    dog = Dog()
    gs_assert(gs_type(animal) == "Animal", "animal type expected Animal, actual {}", gs_type(animal))
    gs_assert(gs_type(dog) == "Dog", "dog type expected Dog, actual {}", gs_type(dog))

    gs_assert(animal.speak() == 1, "animal.speak expected 1, actual {}", animal.speak())
    dog.VoiceFn = animalVoice
    gs_assert(dog.speak() == 1, "dog.speak expected 1 after override, actual {}", dog.speak())

    p = Vec2(9, 8)
    gs_assert(p.x == 9, "Vec2.x expected 9, actual {}", p.x)
    p.y = 23
    gs_assert(p.y == 23, "Vec2.y expected 23, actual {}", p.y)

    e = Entity()
    gs_assert(e.HP == 100, "Entity.HP expected 100, actual {}", e.HP)
    e.HP = 135
    e.MP = 88
    e.Speed = 9
    e.Position = p
    e.GotoPoint(p)
    gs_assert(e.HP == 135, "Entity.HP expected 135, actual {}", e.HP)
    gs_assert(gs_type(id(e)) == "int", "type(id(e)) expected int, actual {}", gs_type(id(e)))
    return 1
```

    </td>
  </tr>
</table>

### `benchmark_hot_loop`

- GS 指令数: **23**  |  Py 指令数: **26**  |  差值(Py-GS): **3**

<table>
  <tr>
    <th align="left" width="50%">GameScript (GS)</th>
    <th align="left" width="50%">Python (Py)</th>
  </tr>
  <tr>
    <td valign="top">

```gs
fn benchmark_hot_loop() {
    let total = 0;
    for (i in range(0, 10000)) {
        total = total + i;
    }
    assert(total == 49995000, "hot loop checksum mismatch: {}", total);
    return total;
}
```

    </td>
    <td valign="top">

```python
def benchmark_hot_loop():
    total = 0
    for i in range(0, 10000):
        total += i
    gs_assert(total == 49995000, "hot loop checksum mismatch: {}", total)
    return total
```

    </td>
  </tr>
</table>

### `benchmark_module_calls`

- GS 指令数: **28**  |  Py 指令数: **31**  |  差值(Py-GS): **3**

<table>
  <tr>
    <th align="left" width="50%">GameScript (GS)</th>
    <th align="left" width="50%">Python (Py)</th>
  </tr>
  <tr>
    <td valign="top">

```gs
fn benchmark_module_calls() {
    let total = 0;
    for (i in range(0, 2000)) {
        total = total + mm.add(i, 1);
    }
    assert(total == 2001000, "module-call benchmark checksum mismatch: {}", total);
    return total;
}
```

    </td>
    <td valign="top">

```python
def benchmark_module_calls():
    total = 0
    for i in range(0, 2000):
        total += mm.add(i, 1)
    gs_assert(total == 2001000, "module-call benchmark checksum mismatch: {}", total)
    return total
```

    </td>
  </tr>
</table>

### `benchmark_iter_traversal`

- GS 指令数: **130**  |  Py 指令数: **104**  |  差值(Py-GS): **-26**

<table>
  <tr>
    <th align="left" width="50%">GameScript (GS)</th>
    <th align="left" width="50%">Python (Py)</th>
  </tr>
  <tr>
    <td valign="top">

```gs
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
```

    </td>
    <td valign="top">

```python
def benchmark_iter_traversal():
    values = GSList()
    for i in range(0, 1000):
        values.push(i)

    tup = GSTuple(1, 2, 3, 4, 5, 6, 7, 8, 9, 10)

    d = GSDict()
    for i in range(0, 1000):
        d[i] = i + 1

    sum_list = 0
    for element in values:
        sum_list += element

    sum_tuple = 0
    for element in tup:
        sum_tuple += element

    sum_dict = 0
    for k, v in d:
        sum_dict += k + v

    checksum = sum_list + sum_tuple + sum_dict
    gs_assert(checksum == 1499555, "iter-traversal benchmark checksum mismatch: {}", checksum)
    return checksum
```

    </td>
  </tr>
</table>

### `benchmark_printf`

- GS 指令数: **19**  |  Py 指令数: **30**  |  差值(Py-GS): **11**

<table>
  <tr>
    <th align="left" width="50%">GameScript (GS)</th>
    <th align="left" width="50%">Python (Py)</th>
  </tr>
  <tr>
    <td valign="top">

```gs
fn benchmark_printf() {
    let i = 100;
    let value = i * 7 + 3;
    printf("[printf-bench] i=%03d hex=%H str={} fp=%.2f\\n", i, value, value, value);
    let checksum = value;
    return checksum;
}
```

    </td>
    <td valign="top">

```python
def benchmark_printf():
    i = 100
    value = i * 7 + 3
    line = "[printf-bench] i=%03d hex=%s str=%s fp=%.2f" % (i, format(value, "X"), value, float(value))
    print(line)
    checksum = value
    return checksum
```

    </td>
  </tr>
</table>

### `run_bench`

- GS 指令数: **127**  |  Py 指令数: **133**  |  差值(Py-GS): **6**

<table>
  <tr>
    <th align="left" width="50%">GameScript (GS)</th>
    <th align="left" width="50%">Python (Py)</th>
  </tr>
  <tr>
    <td valign="top">

```gs
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
    let reclaimed = system.gc();
    assert(reclaimed >= 0, "system.gc should be non-negative, actual {}", reclaimed);

    if (verbose == 1) {
        print("[bench] passed groups", passed);
        print("[bench] checksum1", checksum1);
        print("[bench] checksum2", checksum2);
        print("[bench] checksum3", checksum3);
        print("[bench] checksum4", checksum4);
        print("[bench] gc", reclaimed);
        print("[bench] suite done");
    }

    assert(passed == 6, "passed groups expected 6, actual {}", passed);
    return checksum1 + checksum2 + checksum3 + checksum4 + reclaimed;
}
```

    </td>
    <td valign="top">

```python
def run_bench(verbose):
    if verbose == 1:
        print("[bench] suite start")
        print("gg.hello", gg.hello(), TOP_LEVEL_SENTINEL)

    test_load_const()

    passed = 0
    passed += test_arithmetic_and_string()
    passed += test_list_and_dict()
    passed += test_tuple()
    passed += test_loops_and_control_flow()
    passed += test_modules_and_imports()
    passed += test_classes_and_host_objects()

    checksum1 = benchmark_hot_loop()
    checksum2 = benchmark_module_calls()
    checksum3 = benchmark_iter_traversal()
    checksum4 = benchmark_printf()
    reclaimed = system.gc()
    gs_assert(reclaimed >= 0, "system.gc should be non-negative, actual {}", reclaimed)

    if verbose == 1:
        print("[bench] passed groups", passed)
        print("[bench] checksum1", checksum1)
        print("[bench] checksum2", checksum2)
        print("[bench] checksum3", checksum3)
        print("[bench] checksum4", checksum4)
        print("[bench] gc", reclaimed)
        print("[bench] suite done")

    gs_assert(passed == 6, "passed groups expected 6, actual {}", passed)
    return checksum1 + checksum2 + checksum3 + checksum4 + reclaimed
```

    </td>
  </tr>
</table>

### `ue_entry`

- GS 指令数: **4**  |  Py 指令数: **5**  |  差值(Py-GS): **1**

<table>
  <tr>
    <th align="left" width="50%">GameScript (GS)</th>
    <th align="left" width="50%">Python (Py)</th>
  </tr>
  <tr>
    <td valign="top">

```gs
fn ue_entry() {
    return run_bench(0);
}
```

    </td>
    <td valign="top">

```python
def ue_entry():
    return run_bench(0)
```

    </td>
  </tr>
</table>

### `main`

- GS 指令数: **14**  |  Py 指令数: **15**  |  差值(Py-GS): **1**

<table>
  <tr>
    <th align="left" width="50%">GameScript (GS)</th>
    <th align="left" width="50%">Python (Py)</th>
  </tr>
  <tr>
    <td valign="top">

```gs
fn main() {
    let total = run_bench(1);
    assert(total > 0, "bench total should be positive, actual {}", total);
    return 0;
}
```

    </td>
    <td valign="top">

```python
def main():
    total = run_bench(1)
    gs_assert(total > 0, "bench total should be positive, actual {}", total)
    return 0
```

    </td>
  </tr>
</table>

