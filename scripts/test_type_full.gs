fn main() {
    print("=== Type Object Tests ===");

    let intType = type(1);
    let floatType = type(3.14);
    let boolType = type(true);
    let stringType = type("hello");
    let nullType = type(null);

    print("intType:", intType);
    print("floatType:", floatType);
    print("boolType:", boolType);
    print("stringType:", stringType);
    print("nullType:", nullType);

    print("");
    print("=== Type Conversion Tests ===");

    print("Int(42):", intType(42));
    print("Int(3.14):", intType(3.14));
    print("Int(true):", intType(true));
    print("Int(false):", intType(false));
    print("Int(\"123\"):", intType("123"));
    print("Int(null):", intType(null));

    print("Float(42):", floatType(42));
    print("Float(3.14):", floatType(3.14));
    print("Float(true):", floatType(true));
    print("Float(\"3.14\"):", floatType("3.14"));

    print("Bool(1):", boolType(1));
    print("Bool(0):", boolType(0));
    print("Bool(3.14):", boolType(3.14));
    print("Bool(0.0):", boolType(0.0));
    print("Bool(null):", boolType(null));
    print("Bool(\"hello\"):", boolType("hello"));

    print("String(42):", stringType(42));
    print("String(3.14):", stringType(3.14));
    print("String(true):", stringType(true));
    print("String(null):", stringType(null));

    print("");
    print("=== Practical Usage ===");

    let IntTypeForConvert = type(0);
    print("Int(\"100\"):", IntTypeForConvert("100"));
    print("Int(3.7):", IntTypeForConvert(3.7));
    print("Int(true):", IntTypeForConvert(true));

    let myInt = intType("42");
    let myFloat = floatType(100);
    let myBool = boolType(1);

    print("myInt:", myInt, "type:", type(myInt));
    print("myFloat:", myFloat, "type:", type(myFloat));
    print("myBool:", myBool, "type:", type(myBool));

    print("");
    print("=== All tests passed! ===");
    
    return 0;
}
