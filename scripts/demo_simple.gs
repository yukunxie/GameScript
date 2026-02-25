# Simplified demo test
import object.creatures as creatures;
import misc.flow as flow;
import module_math as mm;
import module_globals as mg;
import system as system;
from module_math import add as plus;
from module_math import add, hello as gg;

fn main() {
    print("Starting simplified demo...");
    
    print("Testing system module...");
    let t1 = system.getTimeMs();
    print("Time:", t1);
    
    print("Testing module_math...");
    let result = mm.add(5, 7);
    print("mm.add(5, 7) =", result);
    
    print("Testing plus alias...");
    let result2 = plus(3, 4);
    print("plus(3, 4) =", result2);
    
    print("Testing gg.hello...");
    print("gg.hello =", gg.hello);
    
    print("Testing Vec2...");
    let v = Vec2(3.0, 4.0);
    print("Vec2(3.0, 4.0).length() =", v.length());
    
    print("Testing Entity...");
    let e = Entity();
    print("Entity HP:", e.HP);
    
    print("Testing GC...");
    let reclaimed = system.gc();
    print("GC reclaimed:", reclaimed);
    
    print("Simplified demo passed!");
    return 0;
}