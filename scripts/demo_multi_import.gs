# Test multiple imports
import system as system;
import module_math as mm;

fn main() {
    print("Multi import test starting...");
    let t = system.getTimeMs();
    print("Time:", t);
    let r = mm.add(5, 7);
    print("mm.add(5, 7) =", r);
    print("Multi import test done!");
    return 0;
}