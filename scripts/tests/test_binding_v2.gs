# Binding V2 Quick Start Test
# This script demonstrates the V2 binding API features

fn test_basic_functions() {
    print("=== Testing Basic Functions ===");
    
    # These would be bound via bindings.function()
    # let result = Add(10, 20);
    # print("Add(10, 20) = " + str(result));
    
    # let greeting = Greet("World");
    # print(greeting);
    
    print("Basic functions test complete\n");
}

fn test_class_binding() {
    print("=== Testing Class Binding ===");
    
    # Vec2 class bound via bindings.beginClass<Vec2>()
    # let v1 = Vec2(3.0, 4.0);
    # print("v1 = (" + str(v1.x) + ", " + str(v1.y) + ")");
    # print("v1.length() = " + str(v1.length()));
    
    # v1.normalize();
    # print("After normalize: length = " + str(v1.length()));
    
    # let v2 = Vec2(10, 20);
    # let v3 = v1.add(v2);
    # print("v3 = (" + str(v3.x) + ", " + str(v3.y) + ")");
    
    print("Class binding test complete\n");
}

fn test_properties() {
    print("=== Testing Properties (Getter/Setter) ===");
    
    # Player class with property bindings
    # let player = Player("Hero");
    # print("Player name: " + player.name);
    
    # player.name = "Champion";
    # print("New name: " + player.name);
    
    # player.health = 100;
    # player.takeDamage(30);
    # print("Health after damage: " + str(player.health));
    
    print("Properties test complete\n");
}

fn test_script_callbacks() {
    print("=== Testing Script Callbacks ===");
    
    # Define a callback function
    fn onEvent(eventName) {
        print("Event received: " + eventName);
        return true;
    }
    
    # Pass callback to C++ (if EventSystem is bound)
    # let events = EventSystem();
    # events.RegisterCallback(onEvent);
    # events.TriggerEvent("PlayerSpawn");
    
    print("Callback test complete\n");
}

fn test_nested_objects() {
    print("=== Testing Nested Objects ===");
    
    # Create player with position (Vec2)
    # let player = Player("Alice");
    # player.moveTo(100.0, 200.0);
    
    # let pos = player.getPosition();
    # print("Player position: (" + str(pos.x) + ", " + str(pos.y) + ")");
    
    print("Nested objects test complete\n");
}

fn test_type_conversions() {
    print("=== Testing Type Conversions ===");
    
    # int32/int64
    # let i32 = ConvertToInt32(42);
    # let i64 = ConvertToInt64(9999999999);
    
    # float/double
    # let f = ConvertToFloat(3.14);
    # let d = ConvertToDouble(2.71828);
    
    # bool
    # let b = ConvertToBool(1);
    
    # string
    # let s = ConvertToString("Hello");
    
    print("Type conversion test complete\n");
}

fn benchmark_binding_overhead() {
    print("=== Benchmarking Binding Overhead ===");
    
    let iterations = 10000;
    let startTime = 0;  # Would use actual timer
    
    # Call native function many times
    # for (i in range(0, iterations)) {
    #     let result = Add(i, i + 1);
    # }
    
    let endTime = 0;
    # print("Time for " + str(iterations) + " calls: " + str(endTime - startTime) + "ms");
    
    print("Benchmark complete\n");
}

fn demonstrate_ue5_style() {
    print("=== UE5-Style API Demo ===");
    
    # FVector usage (if bound)
    # let vec = FVector(100.0, 200.0, 300.0);
    # print("Vector: (" + str(vec.X) + ", " + str(vec.Y) + ", " + str(vec.Z) + ")");
    # print("Length: " + str(vec.Size()));
    
    # let normalized = vec.GetSafeNormal();
    # print("Normalized: (" + str(normalized.X) + ", " + str(normalized.Y) + ", " + str(normalized.Z) + ")");
    
    # AActor usage (if bound)
    # let actor = SpawnActor("Player", vec);
    # let location = actor.GetActorLocation();
    # print("Actor location: (" + str(location.X) + ", " + str(location.Y) + ", " + str(location.Z) + ")");
    
    print("UE5 demo complete\n");
}

fn main() {
    print("========================================");
    print("GameScript Binding V2 - Feature Test");
    print("========================================\n");
    
    test_basic_functions();
    test_class_binding();
    test_properties();
    test_script_callbacks();
    test_nested_objects();
    test_type_conversions();
    benchmark_binding_overhead();
    demonstrate_ue5_style();
    
    print("========================================");
    print("All tests complete!");
    print("========================================");
    
    return 0;
}

# Additional utility functions to test

fn factorial(n) {
    if (n <= 1) {
        return 1;
    }
    return n * factorial(n - 1);
}

fn fibonacci(n) {
    if (n <= 1) {
        return n;
    }
    return fibonacci(n - 1) + fibonacci(n - 2);
}

# These can be called from C++ using ScriptCallableInvoker:
# gs::Value func = runtime.getGlobal("factorial");
# gs::ScriptCallableInvoker invoker(ctx, func);
# int result = invoker.call<int>(5);  // Returns 120
