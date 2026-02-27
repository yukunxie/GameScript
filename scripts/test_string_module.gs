import string;

fn main() {
    print("=== String Module Test ===");
    
    # 测试 format 功能
    print("Testing string.format()...");
    
    let name = "GameScript";
    let version = 1.0;
    let count = 42;
    
    # 测试 printf 风格格式化
    let formatted1 = string.format("Hello, %s! Version: %f, Count: %d", name, version, count);
    print("Printf style:", formatted1);
    
    # 测试 {} 风格格式化 
    let formatted2 = string.format("Hello, {}! Version: {}, Count: {}", name, version, count);
    print("{} style:", formatted2);
    
    # 测试混合风格
    let formatted3 = string.format("Name: {} | Hex: %H | Float: %.2f", name, count, version);
    print("Mixed style:", formatted3);
    
    print("Testing regex compilation...");
    
    # 测试正则表达式编译
    let pattern = string.compile("\\d+");
    print("Pattern created:", pattern);
    
    # 测试搜索功能
    let text1 = "There are 123 apples and 456 oranges";
    let found = pattern.search(text1);
    print("Search in '", text1, "':", found);
    
    let text2 = "No numbers here!";
    let notFound = pattern.search(text2);
    print("Search in '", text2, "':", notFound);
    
    print("Testing match functionality...");
    
    # 现在可以直接调用 pattern.match() 方法
    let matchResult = pattern.match(text1);
    if (matchResult != null) {
        print("Match found:", matchResult);
        print("Match start:", matchResult.start());
        print("Match end:", matchResult.end());
        print("Matched text:", matchResult.matched());
    } else {
        print("No match found");
    }
    
    print("Testing matchAll functionality...");
    
    let allMatches = pattern.matchAll(text1);
    print("All matches count:", typename(allMatches));
    
    # 测试更复杂的正则表达式
    print("Testing complex regex...");
    
    let emailPattern = string.compile("[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}");
    let emailText = "Contact us at info@example.com or support@test.org";
    
    let emailFound = emailPattern.search(emailText);
    print("Email found:", emailFound);
    
    let emailMatch = emailPattern.match(emailText);
    if (emailMatch != null) {
        print("First email:", emailMatch.matched());
    }
    
    print("=== String Module Test Completed ===");
    
    return 1;
}

main();