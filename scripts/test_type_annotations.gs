class TypedHolder {
    value:int = 10;
    anything:any = 1;

    fn __new__(self, seed:int, extra:any, passthrough) {
        self.value = seed;
        self.anything = extra;
        self.anything = passthrough;
        return self;
    }

    fn add(self, x:int, y:any, z) {
        self.value = self.value + x;
        self.anything = y;
        self.anything = z;
        return self.value;
    }
}

fn testFun(param:int, param2:any, param3) {
    let localTyped:int = param;
    let localAny:any = param2;
    localAny = param3;

    let localUntyped = 10;
    localUntyped = 10.5;

    if (typename(localTyped) != "int") {
        return 11;
    }
    return localTyped;
}

fn main() {
    let varAny:any = 10;
    varAny = 10.25;
    varAny = [];

    let varInt:int = 1;
    if (typename(varInt) != "int") {
        return 1;
    }

    let r = testFun(3, [], 9);
    if (r != 3) {
        return 2;
    }

    let obj = TypedHolder(7, 8, "ok");
    if (obj.value != 7) {
        return 3;
    }

    let added = obj.add(5, 99, []);
    if (added != 12) {
        return 4;
    }

    return 0;
}
