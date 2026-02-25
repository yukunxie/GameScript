# Simple operator test
fn main() {
    print("Testing floor division: 17 // 5");
    let result = 17 // 5;
    print("Result:", result);
    assert(result == 3, "Expected 3");
    
    print("Testing modulo: 17 % 5");
    let mod = 17 % 5;
    print("Result:", mod);
    assert(mod == 2, "Expected 2");
    
    print("Testing power: 2 ** 10");
    let power = 2 ** 10;
    print("Result:", power);
    assert(power == 1024, "Expected 1024");
    
    print("All tests passed!");
    return 0;
}
