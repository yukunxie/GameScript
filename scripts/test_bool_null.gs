# Test Bool and Null types with truthiness rules
fn test_bool_literals() {
    print("=== Testing Bool Literals ===");
    
    let t = true;
    let f = false;
    
    print("true literal:", t);
    print("false literal:", f);
    
    if (t) {
        print("✓ true is truthy");
    }
    
    if (!f) {
        print("✓ false is falsy");
    }
}

fn test_null_literal() {
    print("\n=== Testing Null Literal ===");
    
    let n = null;
    print("null literal:", n);
    
    if (!n) {
        print("✓ null is falsy");
    }
    
    # Null comparison
    if (n == null) {
        print("✓ null == null");
    }
    
    let x = 42;
    if (x != null) {
        print("✓ 42 != null");
    }
}

fn test_truthiness() {
    print("\n=== Testing Truthiness Rules ===");
    
    # Integer 0 is false, all other integers are true
    let zero = 0;
    let one = 1;
    let neg = -5;
    
    if (!zero) {
        print("✓ 0 is falsy");
    }
    
    if (one) {
        print("✓ 1 is truthy");
    }
    
    if (neg) {
        print("✓ -5 is truthy");
    }
    
    # Float 0.0 should be falsy
    let fzero = 0.0;
    let fone = 1.0;
    
    if (!fzero) {
        print("✓ 0.0 is falsy");
    }
    
    if (fone) {
        print("✓ 1.0 is truthy");
    }
    
    # null is falsy
    if (!null) {
        print("✓ null is falsy");
    }
    
    # Strings, lists, etc. should be truthy
    let s = "hello";
    let lst = [1, 2, 3];
    
    if (s) {
        print("✓ non-empty string is truthy");
    }
    
    if (lst) {
        print("✓ non-empty list is truthy");
    }
}

fn helper_no_return() {
    let x = 42;
    # No return statement - should return null
}

fn test_implicit_return() {
    print("\n=== Testing Implicit Null Return ===");
    
    let result = helper_no_return();
    print("Function with no return returned:", result);
    
    if (result == null) {
        print("✓ Implicit return is null");
    }
}

fn test_conditional_with_bool() {
    print("\n=== Testing Conditionals with Bool ===");
    
    let flag = true;
    
    if (flag) {
        print("✓ if (true) works");
    }
    
    if (flag == true) {
        print("✓ explicit comparison works");
    }
    
    let count = 5;
    let is_positive = count > 0;
    
    if (is_positive) {
        print("✓ comparison result is truthy");
    }
}

fn main() {
    print("==========================================");
    print("  Bool and Null Type Test Suite");
    print("==========================================\n");
    
    test_bool_literals();
    test_null_literal();
    test_truthiness();
    test_implicit_return();
    test_conditional_with_bool();
    
    print("\n==========================================");
    print("  All tests completed!");
    print("==========================================");
    
    return 0;
}
