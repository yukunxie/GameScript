import system as system;

fn main() {
    print("Testing system module...");
    
    let t1 = system.getTimeMs();
    print("t1 =", t1);
    
    let sum = 0;
    let i = 0;
    while (i < 1000) {
        sum = sum + i;
        i = i + 1;
    }
    
    let t2 = system.getTimeMs();
    print("t2 =", t2);
    print("elapsed =", t2 - t1);
    print("sum =", sum);
    
    let reclaimed = system.gc();
    print("GC reclaimed =", reclaimed);
    
    print("System module test passed!");
    return 0;
}