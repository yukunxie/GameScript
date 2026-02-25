# Simple benchmark test for operators
fn test_operators() {
    # Arithmetic operators
    let floorDiv = 17 // 5;
    assert(floorDiv == 3, "floor division expected 3, actual {}", floorDiv);
    
    let modResult = 17 % 5;
    assert(modResult == 2, "modulo expected 2, actual {}", modResult);
    
    let powerResult = 2 ** 10;
    assert(powerResult == 1024, "power expected 1024, actual {}", powerResult);
    
    # Bitwise operators
    let andResult = 12 & 5;
    assert(andResult == 4, "bitwise AND expected 4, actual {}", andResult);
    
    let orResult = 12 | 5;
    assert(orResult == 13, "bitwise OR expected 13, actual {}", orResult);
    
    let xorResult = 12 ^ 5;
    assert(xorResult == 9, "bitwise XOR expected 9, actual {}", xorResult);
    
    # Logical operators
    let logicalAnd = (5 > 3) && (10 < 20);
    assert(logicalAnd == 1, "logical AND expected 1, actual {}", logicalAnd);
    
    # Membership operators
    let list = [1, 2, 3, 4, 5];
    let inList = 3 in list;
    assert(inList == 1, "membership check expected 1, actual {}", inList);
    
    let notInList = 10 not in list;
    assert(notInList == 1, "not in membership expected 1, actual {}", notInList);
    
    print("All operator tests passed!");
    return 0;
}

fn main() {
    return test_operators();
}
