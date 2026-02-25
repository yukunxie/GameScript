# Test membership operators
fn main() {
    print("Testing membership operators");
    
    let list = [1, 2, 3, 4, 5];
    let inList = 3 in list;
    print("3 in list:", inList);
    assert(inList == 1, "Expected 1");
    
    print("Testing not in");
    let notInList = 10 not in list;
    print("10 not in list:", notInList);
    assert(notInList == 1, "Expected 1");
    
    print("All tests passed!");
    return 0;
}
