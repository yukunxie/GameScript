# Debug-only negative test.
# Expected behavior (Debug build): runtime throws type mismatch.
# Expected behavior (Release build): no type-check throw.

class BadAttr {
    value:int = 1;

    fn __new__(self) {
        self.value = [];
        return self;
    }
}

fn takesInt(x:int) {
    return x;
}

fn main() {
    let a:int = 10;
    a = [];

    let _ = takesInt(1.5);
    let b = BadAttr();
    return 0;
}
