# Minimal import test
import module_math as mm;

fn main() {
    print("Import test starting...");
    let result = mm.add(2, 3);
    print("mm.add(2, 3) =", result);
    print("Import test passed!");
    return 0;
}