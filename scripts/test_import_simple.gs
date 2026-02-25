# Simple test to verify imports work
import system as system;
import module_math as mm;

fn main() {
    print("Testing imports...");
    
    let result = mm.add(2, 3);
    print("mm.add(2, 3) =", result);
    
    let t = system.getTimeMs();
    print("Time:", t);
    
    print("Done!");
    return 0;
}