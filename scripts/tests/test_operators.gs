# GameScript Operator Test Suite
# Test all newly added operators

# ==================== Arithmetic Operators ====================
fn test_arithmetic() {
    printf("=== Arithmetic Operators Test ===\n");
    
    # Basic arithmetic
    let a = 10;
    let b = 3;
    
    printf("Addition: 10 + 3 = %d\n", a + b);
    printf("Subtraction: 10 - 3 = %d\n", a - b);
    printf("Multiplication: 10 * 3 = %d\n", a * b);
    printf("Division: 10 / 3 = %f\n", a / b);
    
    # New: Floor division
    printf("Floor division: 10 // 3 = %d\n", a // b);
    printf("Floor division: -10 // 3 = %d\n", -10 // 3);
    
    # New: Modulo
    printf("Modulo: 10 %% 3 = %d\n", a % b);
    printf("Modulo: -10 %% 3 = %d\n", -10 % 3);
    
    # New: Power
    printf("Power: 2 ** 3 = %d\n", 2 ** 3);
    printf("Power: 10 ** 2 = %d\n", a ** 2);
    printf("Power: 5 ** 0 = %d\n", 5 ** 0);
}

# ==================== Bitwise Operators ====================
fn test_bitwise() {
    printf("=== Bitwise Operators Test ===\n");
    
    let a = 12;  # 1100 in binary
    let b = 5;   # 0101 in binary
    
    # Bitwise AND
    printf("Bitwise AND: 12 & 5 = %d\n", a & b);  # 0100 = 4
    
    # Bitwise OR
    printf("Bitwise OR: 12 | 5 = %d\n", a | b);  # 1101 = 13
    
    # Bitwise XOR
    printf("Bitwise XOR: 12 ^ 5 = %d\n", a ^ b);  # 1001 = 9
    
    # Bitwise NOT
    printf("Bitwise NOT: ~12 = %d\n", ~a);
    
    # Left shift
    printf("Left shift: 5 << 2 = %d\n", b << 2);  # 20
    
    # Right shift
    printf("Right shift: 12 >> 2 = %d\n", a >> 2);  # 3
}

# ==================== Logical Operators ====================
fn test_logical() {
    printf("=== Logical Operators Test ===\n");
    
    let t = 1;
    let f = 0;
    
    # Logical AND
    printf("Logical AND: 1 && 1 = %d\n", t && t);
    printf("Logical AND: 1 && 0 = %d\n", t && f);
    printf("Logical AND: 0 && 0 = %d\n", f && f);
    
    # Logical OR
    printf("Logical OR: 1 || 1 = %d\n", t || t);
    printf("Logical OR: 1 || 0 = %d\n", t || f);
    printf("Logical OR: 0 || 0 = %d\n", f || f);
    
    # Combined
    printf("Combined: (5 > 3) && (2 < 4) = %d\n", (5 > 3) && (2 < 4));
    printf("Combined: (5 < 3) || (2 < 4) = %d\n", (5 < 3) || (2 < 4));
}

# ==================== Comparison Operators ====================
fn test_comparison() {
    printf("=== Comparison Operators Test ===\n");
    
    let a = 10;
    let b = 20;
    
    printf("Equal: 10 == 10 = %d\n", a == 10);
    printf("Not equal: 10 != 20 = %d\n", a != b);
    printf("Greater than: 20 > 10 = %d\n", b > a);
    printf("Less than: 10 < 20 = %d\n", a < b);
    printf("Greater or equal: 10 >= 10 = %d\n", a >= 10);
    printf("Less or equal: 10 <= 20 = %d\n", a <= b);
}

# ==================== Identity Operators ====================
fn test_identity() {
    printf("=== Identity Operators Test ===\n");
    
    let a = 10;
    let b = 10;
    let c = a;
    
    # 'is' checks if same object (for integers, value equality)
    printf("Identity: a is b (10 is 10) = %d\n", a is b);
    printf("Identity: a is c (same var) = %d\n", a is c);
    
    # 'not is'
    let x = 10;
    let y = 20;
    printf("Identity: x not is y (10 not is 20) = %d\n", x not is y);
}

# ==================== Membership Operators ====================
fn test_membership() {
    printf("=== Membership Operators Test ===\n");
    
    # List test
    let list = [1, 2, 3, 4, 5];
    printf("3 in list = %d\n", 3 in list);
    printf("6 in list = %d\n", 6 in list);
    printf("3 not in list = %d\n", 3 not in list);
    printf("6 not in list = %d\n", 6 not in list);
}

# ==================== Operator Precedence Test ====================
fn test_precedence() {
    printf("=== Operator Precedence Test ===\n");
    
    # Power has highest precedence
    printf("2 + 3 ** 2 = %d\n", 2 + 3 ** 2);  # Should be 11 (2 + 9)
    
    # Multiplication/division before addition/subtraction
    printf("2 + 3 * 4 = %d\n", 2 + 3 * 4);  # Should be 14
    
    # Bitwise precedence
    printf("5 | 3 & 6 = %d\n", 5 | 3 & 6);  # & before |
    
    # Comparison before logical
    printf("5 > 3 && 2 < 4 = %d\n", 5 > 3 && 2 < 4);
}

# ==================== Main Function ====================
fn main() {
    printf("==========================================\n");
    printf("  GameScript Operator Test Suite\n");
    printf("==========================================\n");
    printf("\n");
    
    test_arithmetic();
    printf("\n");
    
    test_bitwise();
    printf("\n");
    
    test_logical();
    printf("\n");
    
    test_comparison();
    printf("\n");
    
    test_identity();
    printf("\n");
    
    test_membership();
    printf("\n");
    
    test_precedence();
    printf("\n");
    
    printf("==========================================\n");
    printf("  All tests completed!\n");
    printf("==========================================\n");
}