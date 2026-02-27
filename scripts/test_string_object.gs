fn main() {
    let text = "Hello, World!";
    print("Basic string: ", text);
    print("String length: ", text.length);
    print("String size: ", text.size());

    let result = text.contains("World");
    print("Contains 'World': ", result);

    let pos = text.find("World");
    print("Position of 'World': ", pos);

    let upper = text.upper();
    print("Uppercase: ", upper);

    let lower = text.lower();
    print("Lowercase: ", lower);

    let substr = text.substr(0, 5);
    print("Substring (0,5): ", substr);

    let slice = text.slice(7, 12);
    print("Slice (7,12): ", slice);

    let parts = text.split(", ");
    print("Split on ', ': ", parts);

    let replaced = text.replace("World", "Universe");
    print("Replaced: ", replaced);
}
