import object.creatures as creatures;
import misc.flow as flow;
import system as system;

# fn tick() {
#     return flow.tick();
# }

let agggg = 10;

fn main() {

    print ("hello world", agggg);
    let base = 40 + 2;
    print(base);
    let baseStr = str(base);
    print(baseStr);

    print("loop start");
    let loopTotal = flow.loop_demo();
    print(loopTotal);
    print("loop end");

    let dict = {};
    dict[1] = base;
    print("dict1", dict);
    let oldValue = dict[1];
    print("[index]", oldValue);
    dict[1] = oldValue + 1;
    let removedDict = dict.del(1);
    

    print ("dict", dict);

    let list = [];
    list.push(10);
    list.push(20);
    list.push(5);
    let first = list[0];
    print(first);
    list[1] = base;
    list.sort();
    print("list(sorted)", list);
    let removedList = list.remove(1);
    print(removedList);

    let majorReclaimed = system.gc();
    print("gc(major-default)", majorReclaimed);
    system.mydata = base;
    print("system.mydata", system.mydata);

    # let task = spawn worker(base);
    # let result = await task;

    let animal = creatures.Animal();
    let dog = creatures.Dog();
    print("type(animal)", type(animal));
    print("type(dog)", type(dog));
    print(str(animal));
    print(animal.__str__());
    print(str(dog));
    print(dog.__str__());

    dog.VoiceFn = creatures.animalVoice;

    print(animal.Name);
    print(dog.Name);
    print(animal.speak());
    print("dog.speak", dog.speak());
    print("dog.VoiceFn", dog.VoiceFn());

    let p = Vec2(9, 8);
    
    print(str(p));
    print(p.__str__());
    print(p.x);
    p.y = 23.0;
    print("p", p);
    print("px, py", p.x, p.y);

    let s = "dsjfkdfj";
    print ("str", s);

    let e = Entity();
    print(type(e));
    print(str(e));
    print(e.__str__());
    print(e.HP);
    e.HP = 135;
    e.MP = 88;
    e.GotoPoint(p);
    e.Speed = 9.0;
    e.Position = p;
    print(e.HP);
    print ("id(e)", type(id(e)));

    

    #print(result);
    #return result;
}
