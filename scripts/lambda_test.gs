fn main() {
    let base = 10;
    let adder = (x, y) => {
        let sum = x + y;
        base = base + sum;
        return base;
    };

    let a = adder(1, 2);
    let b = adder(3, 4);
    print(a);
    print(b);
    return b;
}
