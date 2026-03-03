class Base {
    fn __new__(self) {
        self.v = 1;
    }

    fn Print(self)
    {
        print ("base0");
    }
}

class Derived extends Base {
    fn __new__(self) {
        self.v = 2;
    }
}

class DerivedDict extends Dict {
    fn __new__(self) {
        self.k = 0;
    }
}

fn main() {
    let a = Derived();
    let b = Derived();

    let pa = a.__proto__;
    let pb = b.__proto__;
    a.Print();

    # pb.__base__.Print = (self) => {
    #     print ("base1");
    # };
    print ("pa: {}, pb: {}", pa, pb.__base__);
    assert(pa is pb, "same class instances should reuse same TypeObject");
    assert(pa.__base__ != null, "Derived.__base__ should not be null");

    let d = DerivedDict();
    let pd = d.__proto__;
    assert(pd.__base__ != null, "DerivedDict.__base__ should not be null");

    let t = pa.__proto__;
    assert(t != null, "TypeObject should have __proto__");

    print("[test] typeobject proto/base passed");
    return 0;
}
