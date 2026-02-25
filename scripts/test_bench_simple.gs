# Simple benchmark test
import system as system;

fn main() {
    print("Simple benchmark test...");
    
    let iterations = 10000;
    let start = system.getTimeMs();
    
    let sum = 0;
    let i = 0;
    while (i < iterations) {
        sum = sum + i;
        i = i + 1;
    }
    
    let elapsed = system.getTimeMs() - start;
    
    print("Iterations:", iterations);
    print("Sum:", sum);
    print("Elapsed:", elapsed, "ms");
    
    return 0;
}