import os;

fn main() {
    print("=== OS Module Comprehensive Test ===");
    
    # Test getting current working directory
    print("Current working directory:", os.getcwd());
    
    # Test file writing and reading
    let testFile = "test_output.txt";
    let content = "Hello, GameScript OS module!\nThis is a test file.";
    
    print("Writing content to", testFile);
    let bytesWritten = os.write(testFile, content);
    print("Bytes written:", bytesWritten);
    
    print("Reading content from", testFile);
    let readContent = os.read(testFile);
    print("Read content:", readContent);
    
    # Test file existence and properties
    print("File exists:", os.exists(testFile));
    print("Is file:", os.isFile(testFile));
    print("File size:", os.fileSize(testFile));
    
    # Test path operations
    let dirname = os.dirname(testFile);
    let basename = os.basename(testFile);
    let extension = os.extension(testFile);
    
    print("Directory name:", dirname);
    print("Base name:", basename);
    print("Extension:", extension);
    
    # Test path joining
    let newPath = os.join("subdir", "newfile.txt");
    print("Joined path:", newPath);
    
    # Test absolute path
    let absPath = os.abspath(testFile);
    print("Absolute path:", absPath);
    
    # Test path normalization  
    let normalizedPath = os.normalize(".//test//..//normalized.txt");
    print("Normalized path:", normalizedPath);
    
    # Test File object operations
    print("Opening file using File object...");
    let fileObj = os.open(testFile, "r");
    let fileContent = fileObj.read();
    print("Content via File object:", fileContent);
    fileObj.close();
    
    # Test appending
    print("Appending to file...");
    let appendContent = "\nAppended line!";
    os.append(testFile, appendContent);
    let finalContent = os.read(testFile);
    print("Final content after append:", finalContent);
    
    # Test directory operations
    let testDir = "test_directory";
    print("Creating directory:", testDir);
    os.mkdir(testDir);
    print("Directory created, exists:", os.exists(testDir));
    print("Is directory:", os.isDirectory(testDir));
    
    # Cleanup
    print("Cleaning up...");
    os.remove(testFile);
    os.remove(testDir);
    print("Cleanup completed");
    
    print("=== OS Module Test Completed Successfully ===");
    return 1;
}

main();