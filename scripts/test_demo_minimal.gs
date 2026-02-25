# Minimal test to isolate crash
import system as system;

fn main() {
    print("Starting minimal test...");
    
    # Test 1: Basic print
    print("Test 1: Basic print OK");
    
    # Test 2: system module
    let t = system.getTimeMs();
    print("Test 2: system.getTimeMs =", t);
    
    # Test 3: typename
    print("Test 3: typename(42) =", typename(42));
    
    # Test 4: type
    print("Test 4: type(42) =", type(42));
    
    # Test 5: Simple class
    print("Test 5: Creating class...");
    
    print("All minimal tests passed!");
    return 0;
}