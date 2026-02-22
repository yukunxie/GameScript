# fn tick() {
#     let a = 1;
#     yield;
#     let b = a + 2;
#     return b;
# }

fn loop_demo() {
    let total = 0;

    for (i in range(5)) {
        total = total + i;
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

    let whileIndex = 0;
    while (whileIndex < 10) {
        whileIndex = whileIndex + 1;
        if (whileIndex == 3) {
            continue;
        }
        if (whileIndex > 6) {
            break;
        }
        print(whileIndex);
    }

    return total;
}
