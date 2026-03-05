import system as system;

fn explode(n) {
    let z = n - n;
    return n / z;
}

fn main() {
    let iterations = 5000;
    let i = 0;
    let checksum = 0;
    let start = system.getTimeMs();

    while (i < iterations) {
        try {
            explode(i + 1);
        } catch (err) {
            checksum = checksum + err.throw_line + err.throw_column;
        }
        i = i + 1;
    }

    let elapsed = system.getTimeMs() - start;
    print("benchmark_exception_throw_site");
    print("iterations:", iterations);
    print("elapsed_ms:", elapsed);
    print("checksum:", checksum);
    return 0;
}
