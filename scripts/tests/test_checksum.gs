# Test to calculate correct checksum
fn main() {
    let loopSum = 0;
    for (i in range(0, 1000)) {
        loopSum = loopSum + (i % 10) + (i // 10) + ((i & 7) | (i ^ 3));
    }
    print("Loop checksum:", loopSum);
    return 0;
}
