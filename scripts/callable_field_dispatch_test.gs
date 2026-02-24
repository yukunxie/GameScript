import object.creatures as creatures;

fn main() {
    let holder = creatures.Animal();

    holder.Factory = creatures.Dog;
    let dog = holder.Factory();
    assert(type(dog) == "Dog", "holder.Factory() type expected Dog, actual {}", type(dog));

    holder.Logger = print;
    holder.Logger("[callable-field-test] native logger", 42);

    holder.Adder = (x, y) => {
        return x + y;
    };
    assert(holder.Adder(3, 9) == 12, "holder.Adder expected 12, actual {}", holder.Adder(3, 9));

    return 0;
}
