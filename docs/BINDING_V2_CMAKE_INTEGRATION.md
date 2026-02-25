# CMakeLists.txt Integration Guide for Binding V2

## Add to CMakeLists.txt

To integrate the new binding_v2 system into your build, add the following to your CMakeLists.txt:

### Option 1: Add to existing library

```cmake
# In src/CMakeLists.txt or wherever your source files are listed

target_sources(gs_core PRIVATE
    # ... existing sources ...
    binding_v2.cpp
)

target_include_directories(gs_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
```

### Option 2: Create a separate target (recommended for optional use)

```cmake
# New target for V2 bindings
add_library(gs_binding_v2
    src/binding_v2.cpp
)

target_include_directories(gs_binding_v2 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(gs_binding_v2 PUBLIC
    gs_core  # Links to main GameScript library
)

# In your app or executable
target_link_libraries(your_app PRIVATE
    gs_core
    gs_binding_v2  # Optional: only if using V2 API
)
```

### Option 3: Header-only integration (current implementation)

Since `binding_v2.cpp` contains mostly placeholder implementations,
you can use the V2 API as header-only for now:

```cmake
# No changes needed! Just include the header:
# #include "gs/binding_v2.hpp"
```

## For UE5 Projects

If integrating into Unreal Engine 5:

### 1. Add to .Build.cs file

```csharp
// YourModule.Build.cs

PublicIncludePaths.AddRange(new string[] {
    "GameScript/include"
});

PublicDependencyModuleNames.AddRange(new string[] {
    "Core",
    "CoreUObject",
    "Engine",
    "GameScript"  // Your GameScript module
});

// Add binding_v2.cpp to source files
PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "GameScript", "src", "binding_v2.cpp"));
```

### 2. Create Plugin Structure

```
YourProject/Plugins/GameScript/
├── GameScript.uplugin
├── Source/
│   └── GameScript/
│       ├── GameScript.Build.cs
│       ├── Public/
│       │   └── gs/
│       │       ├── binding.hpp
│       │       ├── binding_v2.hpp
│       │       └── ...
│       └── Private/
│           ├── binding.cpp
│           ├── binding_v2.cpp
│           └── ...
```

### 3. Example .uplugin

```json
{
    "FileVersion": 3,
    "Version": 1,
    "VersionName": "2.0.0",
    "FriendlyName": "GameScript",
    "Description": "Lightweight scripting language for Unreal Engine",
    "Category": "Scripting",
    "CreatedBy": "Your Team",
    "CreatedByURL": "",
    "DocsURL": "",
    "MarketplaceURL": "",
    "SupportURL": "",
    "EnabledByDefault": true,
    "CanContainContent": false,
    "IsBetaVersion": true,
    "Installed": false,
    "Modules": [
        {
            "Name": "GameScript",
            "Type": "Runtime",
            "LoadingPhase": "PreDefault"
        }
    ]
}
```

## Build Commands

### Standard CMake Build
```bash
# Configure
cmake --preset default

# Build
cmake --build build --config Release

# The new binding_v2 will be included automatically
```

### UE5 Build
```bash
# Generate project files
GenerateProjectFiles.bat

# Build in Visual Studio or via command line
MSBuild YourProject.sln /p:Configuration=Development
```

## Testing

After building, test the V2 bindings:

```cpp
// In your main.cpp or test file
#include "gs/binding_v2.hpp"

int main() {
    gs::Runtime runtime;
    gs::BindingContext bindings(runtime.host());
    
    // Test function binding
    bindings.function("Test", []() {
        std::cout << "V2 binding works!\n";
    });
    
    runtime.executeString("Test();");
    
    return 0;
}
```

## Troubleshooting

### Compile Errors

**Error**: "Cannot find binding_v2.hpp"
- **Fix**: Ensure `include/gs/` is in include path
- Check `target_include_directories` in CMakeLists.txt

**Error**: "NativeObjectWrapper undefined"
- **Fix**: This is a template class, defined in header
- Make sure to include `#include "gs/binding_v2.hpp"`

**Error**: "Linking error with TypeConverter"
- **Fix**: Template specializations are header-only
- No linking needed for V2 API

### Runtime Errors

**Error**: "ScriptCallableInvoker not implemented"
- **Status**: VM integration pending (next step)
- **Workaround**: Use old API for C++→GS calls for now

**Error**: "HostContext not available in Type callbacks"
- **Status**: Integration pending
- **Workaround**: Use manual binding for properties that need context

## Next Steps

1. **Complete VM Integration**
   - Implement `ScriptCallableInvoker::invokeInternal()`
   - Connect to VM execution engine

2. **Add to CI/CD**
   - Include binding_v2 in automated tests
   - Add performance benchmarks

3. **Documentation**
   - Generate Doxygen docs for V2 API
   - Add tutorial videos

4. **Examples**
   - Create full UE5 integration example
   - Add game sample using V2 bindings

---

**Date**: 2026-02-25  
**Status**: Build integration ready, VM integration pending
