# Test demo with C++ bindings
import system as system;

fn main() {
    print("Testing C++ bindings...");
    
    # Test Vec2 creation and methods
    let v1 = Vec2(3.0, 4.0);
    print("v1:", v1);
    print("v1.x:", v1.x);
    print("v1.y:", v1.y);
    print("v1.length():", v1.length());
    
    let v2 = Vec2(1.0, 2.0);
    let v3 = v1.add(v2);
    print("v1.add(v2):", v3);
    
    # Test Entity creation
    let e = Entity();
    print("Entity HP:", e.HP);
    print("Entity MP:", e.MP);
    e.HP = 150;
    print("Entity HP after set:", e.HP);
    
    # Test Distance function
    let dist = Distance(v1, v2);
    print("Distance(v1, v2):", dist);
    
    print("C++ binding test passed!");
    return 0;
}