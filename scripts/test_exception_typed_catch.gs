class MyException extends Exception {
    fn __new__(self, message) {
        self.message = message;
    }
}

fn main() {
    let score = 0;

    try {
        throw MyException("typed-catch");
    } catch (DivideByZeroException as err) {
        return 11;
    } catch (MyException as err) {
        if (err.message != "typed-catch") {
            return 12;
        }
        score = score + 1;
    } finally {
        score = score + 1;
    }

    try {
        throw Exception("base-catch");
    } catch (MyException as err) {
        return 13;
    } catch (Exception as err) {
        if (err.message != "base-catch") {
            return 14;
        }
        score = score + 1;
    }

    if (score != 3) {
        return 15;
    }

    return 0;
}
