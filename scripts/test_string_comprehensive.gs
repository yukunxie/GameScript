import string;

fn main() {
    print("=== String Module Test ===");
    
    # ============================================
    # 测试 string.format() 基本功能
    # ============================================
    print("\n--- Basic Format Testing ---");
    
    let name = "GameScript";
    let age = 25;
    let height = 175.5;
    
    print("Basic formatting:");
    print(string.format("Name: %s, Age: %d, Height: %.1f", name, age, height));
    print(string.format("Status: {} (age: {})", name, age));
    
    # 数字格式化选项
    print("\nNumber formatting:");
    let num = 255;
    print(string.format("Decimal: %d", num));
    print(string.format("Hex (lowercase): %h", num));  
    print(string.format("Hex (uppercase): %H", num));
    print(string.format("Zero-padded: %04d", num));
    
    # 浮点数格式化
    let pi = 3.14159265;
    print(string.format("Pi: %.2f", pi));
    print(string.format("Pi (6 decimals): %.6f", pi));
    
    # ============================================
    # 测试正则表达式功能
    # ============================================
    print("\n--- Regex Testing ---");
    
    # 测试数字匹配
    let numberPattern = string.compile("\\d+");
    let text1 = "I have 100 apples, 200 oranges, and 50 bananas.";
    
    print("Text:", text1);
    print("Search result:", numberPattern.search(text1));
    
    # 匹配第一个数字
    let firstMatch = numberPattern.match(text1);
    if (firstMatch != null) {
        print("First match:", firstMatch.matched(), "at position", firstMatch.start(), "to", firstMatch.end());
    }
    
    # 匹配所有数字
    let allNumbers = numberPattern.matchAll(text1);
    print("All numbers found (type):", typename(allNumbers));
    
    # 测试邮箱匹配  
    print("\nEmail pattern testing:");
    let emailPattern = string.compile("[\\w._%+-]+@[\\w.-]+\\.[a-zA-Z]{2,}");
    let emailText = "Contact: john.doe@example.com, support@test.org, admin@company.net";
    
    print("Text:", emailText);
    print("Email found:", emailPattern.search(emailText));
    
    let firstEmail = emailPattern.match(emailText);
    if (firstEmail != null) {
        print("First email:", firstEmail.matched());
    }
    
    # 测试日期匹配
    print("\nDate pattern testing:");
    let datePattern = string.compile("\\d{4}-\\d{2}-\\d{2}");
    let dateText = "Events: 2024-01-15, 2024-03-22, and 2024-12-31";
    
    print("Text:", dateText);  
    print("Date pattern matches:", datePattern.search(dateText));
    
    let firstDate = datePattern.match(dateText);
    if (firstDate != null) {
        print("First date:", firstDate.matched());
    }
    
    # 测试单词边界
    print("\nWord boundary testing:");
    let wordPattern = string.compile("\\btest\\b");  # 完整单词 "test"
    let testText1 = "This is a test case";
    let testText2 = "This is testing phase";
    
    print("Text 1:", testText1, "-> Found:", wordPattern.search(testText1));
    print("Text 2:", testText2, "-> Found:", wordPattern.search(testText2));
    
    # 测试不区分大小写 (flags = 1)
    print("\nCase insensitive testing:");
    let casePattern = string.compile("[A-Z]+", 1);  # 大写字母，不区分大小写
    let caseText = "hello WORLD test";
    print("Text:", caseText, "-> Found:", casePattern.search(caseText));
    
    let caseMatch = casePattern.match(caseText);
    if (caseMatch != null) {
        print("Match:", caseMatch.matched());
    }
    
    # ============================================
    # 综合示例
    # ============================================
    print("\n--- Comprehensive Example ---");
    
    let logEntry = "2024-01-15 14:30:22 [ERROR] User 'admin@example.com' failed login from IP 192.168.1.100";
    print("Log entry:", logEntry);
    
    # 提取时间戳
    let timestampPattern = string.compile("\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2}");
    let timestamp = timestampPattern.match(logEntry);
    if (timestamp != null) {
        print("Timestamp:", timestamp.matched());
    }
    
    # 提取邮箱
    let logEmailPattern = string.compile("[\\w._%+-]+@[\\w.-]+\\.[a-zA-Z]{2,}");
    let logEmail = logEmailPattern.match(logEntry);
    if (logEmail != null) {
        print("Email:", logEmail.matched());
    }
    
    # 提取IP地址
    let ipPattern = string.compile("\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}");
    let ip = ipPattern.match(logEntry);
    if (ip != null) {
        print("IP Address:", ip.matched());
    }
    
    print("\n=== String Module Test Completed Successfully! ===");
    
    return 1;
}

main();