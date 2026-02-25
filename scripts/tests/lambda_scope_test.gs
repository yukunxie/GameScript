let globalBase = 100;

fn main() {
    let base = 10;

    let useInnerLet = (x) => {
        let base = x + 1;
        return base;
    };

    let captureAndMutate = (x) => {
        base = base + x;
        return base;
    };

    let useGlobalAndBuiltin = (x) => {
        print(x);
        return globalBase + x;
    };

    let r1 = useInnerLet(5);
    let r2 = captureAndMutate(3);
    let r3 = captureAndMutate(2);
    let r4 = useGlobalAndBuiltin(7);

    print(r1);
    print(r2);
    print(r3);
    print(r4);

    return r4;
}
