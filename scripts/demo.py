import gc

TOP_LEVEL_SENTINEL = 314


def gs_assert(cond, msg, *args):
    if not cond:
        raise AssertionError(msg.format(*args))


def gs_type(value):
    if isinstance(value, float):
        return "float"
    if isinstance(value, int):
        return "int"
    if isinstance(value, str):
        return "string"
    if isinstance(value, GSTuple):
        return "Tuple"
    if isinstance(value, Animal):
        return value.__class__.__name__
    if isinstance(value, Dog):
        return value.__class__.__name__
    return type(value).__name__


class GSList:
    def __init__(self, items=None):
        self.data = list(items or [])

    def push(self, value):
        self.data.append(value)

    def sort(self):
        self.data.sort()

    def size(self):
        return len(self.data)

    def remove(self, index):
        return self.data.pop(index)

    def __getitem__(self, index):
        return self.data[index]

    def __setitem__(self, index, value):
        self.data[index] = value

    def __iter__(self):
        return iter(self.data)


class GSDict:
    def __init__(self, values=None):
        self.data = dict(values or {})

    def size(self):
        return len(self.data)

    def del_(self, key):
        return self.data.pop(key)

    def __getitem__(self, key):
        return self.data[key]

    def __setitem__(self, key, value):
        self.data[key] = value

    def __iter__(self):
        return iter(self.data.items())


class GSTuple:
    def __init__(self, *values):
        self.data = list(values)

    def size(self):
        return len(self.data)

    def __getitem__(self, index):
        return self.data[index]

    def __setitem__(self, index, value):
        self.data[index] = value

    def __iter__(self):
        return iter(self.data)


class mm:
    @staticmethod
    def add(a, b):
        return a + b


class mg:
    G = 123

    @staticmethod
    def getG():
        return mg.G

    @staticmethod
    def getDouble():
        return mg.G + mg.G


def plus(a, b):
    return mm.add(a, b)


class gg:
    @staticmethod
    def hello():
        print("hello from module_math")
        return 7


def flow_loop_demo():
    total = 0
    for i in range(5):
        total += i
    print(total)

    for i in range(3, 7):
        print(i)

    values = GSList([10, 20, 30])
    for item in values:
        print(item)

    while_index = 0
    while while_index < 10:
        while_index += 1
        if while_index == 3:
            continue
        if while_index > 6:
            break
        print(while_index)

    return total


class flow:
    @staticmethod
    def loop_demo():
        return flow_loop_demo()


def animalVoice():
    print("animal")
    return 1


def dogVoice():
    print("dog")
    return 2


class Animal:
    def __init__(self):
        self.Name = "Cat"
        self.VoiceFn = animalVoice

    def __delete__(self):
        print("__delete__ Animal", self.Name)
        return 0

    def speak(self):
        print(self.Name)
        return self.VoiceFn()


class Dog(Animal):
    def __init__(self):
        super().__init__()
        self.Name = "Dog"
        self.VoiceFn = dogVoice


class Vec2:
    def __init__(self, x, y):
        self.x = x
        self.y = y


class Entity:
    def __init__(self):
        self.HP = 100
        self.MP = 0
        self.Speed = 0
        self.Position = None

    def GotoPoint(self, p):
        self.Position = p


def test_load_const():
    k = 2
    k = k + 2 + 2 - 3 * 4 / 9
    print("test_load_const k =", k)


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


def test_tuple():
    t = GSTuple(10, 20, 30)
    gs_assert(gs_type(t) == "Tuple", "tuple type expected Tuple, actual {}", gs_type(t))
    gs_assert(t.size() == 3, "tuple size expected 3, actual {}", t.size())
    gs_assert(t[1] == 20, "tuple[1] expected 20, actual {}", t[1])

    t[1] = 99
    gs_assert(t[1] == 99, "tuple[1] expected 99 after set, actual {}", t[1])
    gs_assert(t.size() == 3, "tuple size must stay 3, actual {}", t.size())
    return 1


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


def test_modules_and_imports():
    gs_assert(TOP_LEVEL_SENTINEL == 314, "top-level sentinel mismatch: {}", TOP_LEVEL_SENTINEL)
    gs_assert(mg.G == 123, "module global G expected 123, actual {}", mg.G)
    gs_assert(mg.getG() == 123, "mg.getG expected 123, actual {}", mg.getG())
    gs_assert(mg.getDouble() == 246, "mg.getDouble expected 246, actual {}", mg.getDouble())

    gs_assert(mm.add(2, 3) == 5, "mm.add expected 5, actual {}", mm.add(2, 3))
    gs_assert(plus(4, 5) == 9, "plus expected 9, actual {}", plus(4, 5))
    gs_assert(flow.loop_demo() == 10, "flow.loop_demo expected 10, actual {}", flow.loop_demo())
    return 1


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


def benchmark_hot_loop():
    total = 0
    for i in range(0, 10000):
        total += i
    gs_assert(total == 49995000, "hot loop checksum mismatch: {}", total)
    return total


def benchmark_module_calls():
    total = 0
    for i in range(0, 2000):
        total += mm.add(i, 1)
    gs_assert(total == 2001000, "module-call benchmark checksum mismatch: {}", total)
    return total


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


def benchmark_printf():
    i = 100
    value = i * 7 + 3
    line = "[printf-bench] i=%03d hex=%s str=%s fp=%.2f" % (i, format(value, "X"), value, float(value))
    print(line)
    checksum = value
    return checksum


class system:
    @staticmethod
    def gc():
        reclaimed = gc.collect()
        return reclaimed if reclaimed >= 0 else 0


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


def ue_entry():
    return run_bench(0)


def main():
    total = run_bench(1)
    gs_assert(total > 0, "bench total should be positive, actual {}", total)
    return 0


if __name__ == "__main__":
    print(f"main() -> {main()}")
