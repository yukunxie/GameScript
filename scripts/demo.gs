fn worker(v) {
    sleep 10;
    return v * 2;
}

fn animalVoice(self) {
    print("animal");
    return 1;
}

fn dogVoice(self) {
    print("dog");
    return 2;
}

class Animal {
    Name = "Cat";
    VoiceFn = animalVoice;

    fn speak(self) {
        print(self.Name);
        return self.VoiceFn();
    }
}

class Dog extends Animal {
    Name = "Dog";
    VoiceFn = dogVoice;

    fn speak(self) {
        print(self.Name);
        return self.VoiceFn();
    }
}

fn tick() {
    let a = 1;
    yield;
    let b = a + 2;
    return b;
}

fn loop_demo() {
    let total = 0;

    for (i in range(5)) {
        let total = total + i;
    }
    print(total);

    for (i in range(3, 7)) {
        print(i);
    }

    let list = [10, 20, 30];
    for (k in list) {
        print(k);
    }

    let dict = {1: 100, 2: 200};
    for (k, v in dict) {
        print(k);
        print(v);
    }

    let state = 2;
    if (state == 1) {
        print(111);
    } elif (state == 2) {
        print(222);
    } else {
        print(333);
    }

    let i = 0;
    while (i < 10) {
        let i = i + 1;
        if (i == 3) {
            continue;
        }
        if (i > 6) {
            break;
        }
        print(i);
    }

    return total;
}

fn main() {
    let base = 40 + 2;
    print(base);
    let baseStr = str(base);
    print(baseStr);

    print("loop start");
    let loopTotal = loop_demo();
    print(loopTotal);
    print("loop end");

    let dict = {};
    dict.set(1, base);
    let oldValue = dict.get(1);
    print(oldValue);
    dict.set(1, oldValue + 1);
    let removedDict = dict.del(1);
    print(removedDict);

    let list = [];
    list.push(10);
    list.push(20);
    let first = list.get(0);
    print(first);
    list.set(1, base);
    let removedList = list.remove(1);
    print(removedList);

    let task = spawn worker(base);
    let result = await task;

    let animal = Animal();
    let dog = Dog();

    dog.VoiceFn = animalVoice;

    print(animal.Name);
    print(dog.Name);
    print(animal.speak());
    print("dog.speak", dog.speak());
    print("dog.VoiceFn", dog.VoiceFn());

    let p = Vec2();
    print(p.x);
    p.x = 10.0;
    p.y = 20.0;
    print(p.x);
    print(p.y);

    let e = Entity();
    print(e.hp);
    e.hp = 135;
    e.mp = 88;
    e.speed = 9.0;
    e.x = p.x;
    e.y = p.y;
    print(e.hp);
    print(e.x);
    print(e.y);

    print(result);
    return result;
}
