fn main() {
    try {
        throw "boom";
    } catch (any as err) {
        print("caught:", err);
    } finally {
        print("finally:done");
    }

    try {
        print("no throw");
    } finally {
        print("finally:no_throw");
    }

    try {
        try {
            throw 42;
        } catch (any as inner) {
            print("inner:", inner);
            throw;
        }
    } catch (any as outer) {
        print("outer:", outer);
    }

    return 0;
}
