print("=== Type Object Tests ===");

let intType = type(1);
let floatType = type(3.14);
let boolType = type(true);
let stringType = type("hello");

print("intType:", intType);
print("floatType:", floatType);
print("boolType:", boolType);
print("stringType:", stringType);

print("");
print("=== Type Conversion Tests ===");

print("Int(42):", intType(42));
print("Int(3.14):", intType(3.14));
print("Int(true):", intType(true));
print("Int(false):", intType(false));

print("Float(42):", floatType(42));
print("Float(3.14):", floatType(3.14));

print("Bool(1):", boolType(1));
print("Bool(0):", boolType(0));

print("String(42):", stringType(42));
print("String(3.14):", stringType(3.14));
print("String(true):", stringType(true));

print("");
print("=== All tests passed! ===");
