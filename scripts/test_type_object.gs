// Test script for TypeObject functionality

print("=== Type Object Tests ===");

// Get type objects
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

// Test Int conversions
print("Int(42):", intType(42));
print("Int(3.14):", intType(3.14));
print("Int(true):", intType(true));
print("Int(false):", intType(false));
print("Int(\"123\"):", intType("123"));
print("Int(null):", intType(null));

// Test Float conversions
print("Float(42):", floatType(42));
print("Float(3.14):", floatType(3.14));
print("Float(true):", floatType(true));
print("Float(\"3.14\"):", floatType("3.14"));

// Test Bool conversions
print("Bool(1):", boolType(1));
print("Bool(0):", boolType(0));
print("Bool(3.14):", boolType(3.14));
print("Bool(0.0):", boolType(0.0));
print("Bool(null):", boolType(null));
print("Bool(\"hello\"):", boolType("hello"));

// Test String conversions
print("String(42):", stringType(42));
print("String(3.14):", stringType(3.14));
print("String(true):", stringType(true));
print("String(null):", stringType(null));

print("");
print("=== Practical Usage ===");

// Function that accepts any type and converts to int
function toInt(value) {
    let IntType = type(0);
    return IntType(value);
}

print("toInt(\"100\"):", toInt("100"));
print("toInt(3.7):", toInt(3.7));
print("toInt(true):", toInt(true));

// Create values using type objects
let myInt = intType("42");
let myFloat = floatType(100);
let myBool = boolType(1);

print("myInt:", myInt, "type:", type(myInt));
print("myFloat:", myFloat, "type:", type(myFloat));
print("myBool:", myBool, "type:", type(myBool));

print("");
print("=== All tests passed! ===");
