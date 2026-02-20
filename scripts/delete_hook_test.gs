class D {
    v = 0;

    fn __new__(self, n) {
        self.v = n;
        return 0;
    }

    fn __delete__(self) {
        print("deleted", self.v);
        return 0;
    }
}

fn main() {
    let a = D(9);
    return 0;
}
