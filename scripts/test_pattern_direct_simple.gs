import string;

fn main() {
    print("=== 测试 PatternType 直接方法绑定 ===");

    # 编译正则表达式
    let pattern = string.compile("\\d+");
    print("编译了正则表达式: " + str(pattern));

    # 测试 search 方法
    let text = "The answer is 42 and 13";
    print("测试文本: '" + text + "'");

    let searchResult = pattern.search(text);
    print("pattern.search() 结果: " + str(searchResult));

    # 测试 match 方法 (直接调用)
    print("\n=== 测试直接调用 pattern.match() ===");
    let match = pattern.match(text);
    if (match != null) {
        print("匹配成功!");
        print("匹配内容: '" + match.matched() + "'");
        print("开始位置: " + str(match.start()));
        print("结束位置: " + str(match.end()));
    } else {
        print("没有匹配");
    }

    # 测试 matchAll 方法 (直接调用)
    print("\n=== 测试直接调用 pattern.matchAll() ===");
    let matches = pattern.matchAll(text);
    print("找到 " + str(matches.size()) + " 个匹配:");
    let i = 0;
    while (i < matches.size()) {
        let currentMatch = matches.get(i);
        print("  [" + str(i) + "] '" + currentMatch.matched() + "' 位置: " + str(currentMatch.start()) + "-" + str(currentMatch.end()));
        i = i + 1;
    }

    print("\n=== PatternType 直接方法测试完成 ===");
}
