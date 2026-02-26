from os import getcwd, write, read, exists, isFile, fileSize, dirname, basename, extension, join, remove, open;

fn main() {
    print("=== OS Module Test ===");
    
    // Test basic path operations
    let currentDir = getcwd();
    print("Current directory: " + currentDir);
    
    // Test path joining
    let testPath = join(currentDir, "test.txt");
    print("Test path: " + testPath);
    
    // Test file operations  
    print("Writing test file...");
    let content = "Hello, GameScript OS module!";
    let bytesWritten = write(testPath, content);
    print("Bytes written: " + bytesWritten);
    
    print("Reading test file...");
    let readContent = read(testPath);
    print("Read content: " + readContent);
    
    // Test file existence
    let fileExists = exists(testPath);
    print("File exists: " + fileExists);
    
    let fileIsFile = isFile(testPath);
    print("Is file: " + fileIsFile);
    
    let fileSizeValue = fileSize(testPath);
    print("File size: " + fileSizeValue);
    
    // Test path operations
    let dirnameValue = dirname(testPath);
    print("Directory name: " + dirnameValue);
    
    let basenameValue = basename(testPath);
    print("Base name: " + basenameValue);
    
    let extensionValue = extension(testPath);
    print("Extension: " + extensionValue);
    
    // Test File object
    print("Testing File object...");
    let file = open(testPath, "r");
    let fileContent = file.read();
    print("File content via File object: " + fileContent);
    file.close();
    
    // Cleanup
    print("Cleaning up...");
    remove(testPath);
    print("File removed");
    
    print("=== OS Module Test Complete ===");
}

main();