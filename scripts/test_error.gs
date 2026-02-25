# Test error logging with stack trace

fn test_function() {
    # This will cause a runtime error to test stack trace
    let dict = {};
    dict["nonexistent_key"];  # Will succeed, but let's cause division by zero
    let x = 10 / 0;  # This should trigger an error
    return x;
}

fn main() {
    print("Testing error logging...");
    test_function();
    return 0;
}