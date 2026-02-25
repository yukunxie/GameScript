# Test not in
fn main() {
    let list = [1, 2, 3];
    
    print("Testing 1 in list");
    let r1 = 1 in list;
    print("Result:", r1);
    
    print("Testing 10 in list");
    let r2 = 10 in list;
    print("Result:", r2);
    
    print("Testing 1 not in list");
    let r3 = 1 not in list;
    print("Result:", r3);
    
    print("Testing 10 not in list");
    let r4 = 10 not in list;
    print("Result:", r4);
    
    return 0;
}
