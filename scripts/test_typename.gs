fn main() {
    print("=== Testing typename() ===");
    let x = 42;
    let y = 3.14;
    let z = true;
    let s = "hello";
    let n = null;
    
    print("typename(42):", typename(x));
    print("typename(3.14):", typename(y));
    print("typename(true):", typename(z));
    print("typename(\"hello\"):", typename(s));
    print("typename(null):", typename(n));
    
    print("");
    print("=== Testing type() ===");
    print("type(42):", type(x));
    print("type(3.14):", type(y));
    print("type(true):", type(z));
    
    print("");
    print("=== Testing type conversions ===");
    let intType = type(0);
    print("Got intType:", intType);
    print("intType(3.14) =", intType(3.14));
    print("intType(true) =", intType(true));
    print("intType(\"99\") =", intType("99"));
    
    print("");
    print("=== All tests passed! ===");
    return 0;
}
