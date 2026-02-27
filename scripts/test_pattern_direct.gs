#!/usr/bin/env gs
# 测试直接在 PatternType 中实现的 match 和 matchAll 方法

import string

println("=== 测试 PatternType 直接方法绑定 ===")

# 编译正则表达式
pattern = string.compile("\\d+")
println("编译了正则表达式: " + str(pattern))

# 测试 search 方法
text = "The answer is 42 and 13"
println("测试文本: '" + text + "'")

searchResult = pattern.search(text)
println("pattern.search() 结果: " + str(searchResult))

# 测试 match 方法 (直接调用)
println("\n=== 测试直接调用 pattern.match() ===")
match = pattern.match(text)
if match != nil {
    println("匹配成功!")
    println("匹配内容: '" + match.matched() + "'")
    println("开始位置: " + str(match.start()))
    println("结束位置: " + str(match.end()))
} else {
    println("没有匹配")
}

# 测试 matchAll 方法 (直接调用)
println("\n=== 测试直接调用 pattern.matchAll() ===")
matches = pattern.matchAll(text)
println("找到 " + str(matches.size()) + " 个匹配:")
for i = 0; i < matches.size(); i++ {
    match = matches.get(i)
    println("  [" + str(i) + "] '" + match.matched() + "' 位置: " + str(match.start()) + "-" + str(match.end()))
}

# 测试不同的正则表达式
println("\n=== 测试更复杂的正则表达式 ===")
emailPattern = string.compile("[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}")
emailText = "联系我们: alice@example.com 或 bob123@test.org"

emailMatches = emailPattern.matchAll(emailText)
println("在文本 '" + emailText + "' 中找到 " + str(emailMatches.size()) + " 个邮箱:")
for i = 0; i < emailMatches.size(); i++ {
    match = emailMatches.get(i)
    println("  邮箱: " + match.matched())
}

println("\n=== PatternType 直接方法测试完成 ===")