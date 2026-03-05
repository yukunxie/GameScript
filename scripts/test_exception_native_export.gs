class MyException extends Exception {
    fn __new__(self, message) {
        self.message = message;
    }
}

fn main() {
    try {
        throw Exception("my exception");
    } catch (err) {
        if (err.name != "Exception") {
            return 1;
        }
        if (err.message != "my exception") {
            return 2;
        }
    }

    try {
        throw MyException("custom exception");
    } catch (err2) {
        if (err2.name != "MyException") {
            return 3;
        }
        if (err2.message != "custom exception") {
            return 4;
        }
    }

    return 0;
}
