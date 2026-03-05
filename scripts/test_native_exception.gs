class CustomException extends Exception {
    fn __new__(self, message) {
        self.message = message;
    }
}

fn main() {
    try {
        a = 1 / 0;
    } catch (DivideByZeroException as err) {
        print("div_name:", err.name);
        print("div_message:", err.message);
    }

    try {
        throw CustomException("custom-boom");
    } catch (CustomException as err2) {
        print("custom_name:", err2.name);
        print("custom_message:", err2.message);
    }

    try {
        let d = {};
        d.get("missing_key");
    } catch (DictKeyNotFoundException as err3) {
        print("dict_name:", err3.name);
        print("dict_message:", err3.message);
    }

    return 0;
}
