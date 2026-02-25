# Test storing function in dictionary - detailed debug

fn my_func() {
    return 42;
}

fn main() {
    print("=== Test: Function Storage in Dict ===");
    
    # Test 1: Direct call
    let result1 = my_func();
    printf("1. Direct call result: %d\\n", result1);
    
    # Test 2: Store in variable and call
    let f = my_func;
    printf("2. Type of f: %s\\n", type(f));
    let result2 = f();
    printf("   Variable call result: %d\\n", result2);
    
    # Test 3: Integer key dict
    print("3. Testing integer key dict...");
    let int_dict = {};
    int_dict[1] = my_func;
    printf("   Type of int_dict[1]: %s\\n", type(int_dict[1]));
    let fn_int = int_dict[1];
    let result3 = fn_int();
    printf("   Int-key dict call result: %d\\n", result3);
    
    # Test 4: String key dict with set method
    print("4. Testing string key dict with set()...");
    let str_dict = {};
    str_dict.set("fn", my_func);
    let fn_str = str_dict.get("fn");
    printf("   Type of str_dict.get('fn'): %s\\n", type(fn_str));
    if (type(fn_str) != "<type 'Null'>") {
        let result4 = fn_str();
        printf("   String-key dict call result: %d\\n", result4);
    } else {
        print("   ERROR: Function became Null!");
    }
    
    # Test 5: String key dict with literal syntax
    print("5. Testing dict literal with string key...");
    let literal_dict = {
        "fn": my_func
    };
    let fn_lit = literal_dict["fn"];
    printf("   Type of literal_dict['fn']: %s\\n", type(fn_lit));
    if (type(fn_lit) != "<type 'Null'>") {
        let result5 = fn_lit();
        printf("   Literal dict call result: %d\\n", result5);
    } else {
        print("   ERROR: Function became Null!");
    }
    
    print("=== Test Complete ===");
}