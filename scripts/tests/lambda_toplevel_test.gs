let scale = 3;
let mul = (x) => {
    return x * scale;
};

fn main() {
    let r1 = mul(5);
    let r2 = mul(7);
    print(r1);
    print(r2);
    return r2;
}
