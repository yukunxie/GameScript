import module_globals as mg;
from module_globals import getG as gg;

fn main() {
    print(type(mg));
    print(mg.G);
    print(mg.getG());
    print(gg());
    return mg.getDouble();
}
