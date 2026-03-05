# Test error logging with stack trace

fn test_function() {
    # This will cause a runtime error to test stack trace
    let warmup = 1 + 2;
    let x = 10 / 0;  # This should trigger an error
    return warmup + x;
}

fn main() {
    print("Testing error logging...");
    test_function();
    return 0;
}