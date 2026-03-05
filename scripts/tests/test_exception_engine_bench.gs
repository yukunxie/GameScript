import system as system;

fn main() {
    let iterations = 5000;
    let checksum = 0;
    let i = 0;
    let start = system.getTimeMs();

    while (i < iterations) {
        try {
            let z = i - i;
            let x = 1 / z;
            checksum = checksum + x;
        } catch (DivideByZeroException as err) {
            checksum = checksum + err.throw_line + err.throw_column;
        }
        i = i + 1;
    }

    let elapsed = system.getTimeMs() - start;
    print("test_exception_engine_bench");
    print("iterations:", iterations);
    print("elapsed_ms:", elapsed);
    print("checksum:", checksum);
    assert(checksum > 0, "checksum should be positive, actual {}", checksum);
    return 0;
}
