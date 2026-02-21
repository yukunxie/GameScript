# fn tick() {
#     let a = 1;
#     yield;
#     let b = a + 2;
#     return b;
# }

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
