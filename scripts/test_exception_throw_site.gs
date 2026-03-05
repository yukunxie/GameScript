
class Test
{
    fn __new__(self) {
        print ("Test constructor");
    }

    fn explode(self) {
        # throw Exception("explode function");
        let x = 1 / 0;
        return x;
    }

    fn invoke_explode(self) {
        try {  
            self.explode();
        } catch (err) {
            print ("caught in invoke_explode, re-throwing");
            throw err;
        } finally {
            print ("finally in invoke_explode");
        }
    }
}

   

fn main() {
    try {
        let test = Test();
        test.invoke_explode();
    } catch (any as err) {
        print("name:", err.name);
        print("message:", err.message);
        print("throw_function:", err.throw_function);
        print("throw_line:", err.throw_line);
        print("throw_column:", err.throw_column);
        print("throw_stack:\n" + err.throw_stack);

        if (err.name != "DivideByZeroException") {
            return 11;
        }
        if (err.throw_function != "explode" && err.throw_function != "Test::explode") {
            return 12;
        }
        if (err.throw_line <= 0) {
            return 13;
        }
        if (err.throw_column <= 0) {
            return 14;
        }
        if (err.throw_stack == "") {
            return 15;
        }
    } finally {
        print("finally block executed");
    }

    return 0;
}
