# Format String Support Implementation Summary

## Overview
This implementation adds optional support for modern C++ formatting (std::format or fmtlib) to the http_io examples, addressing issue: "need fmtlib or std::format or both".

## Solution Architecture

### Conditional Compilation Strategy
The solution uses a tiered approach:
1. **Preferred**: C++20 `std::format` (via `std::vformat` for runtime format strings)
2. **Fallback**: fmtlib if available (auto-detected via `__has_include(<fmt/core.h>)`)
3. **Legacy**: Original `std::stringstream` concatenation for maximum compatibility

### Key Benefits
- **Zero Breaking Changes**: Existing code continues to work unchanged
- **No Hard Dependencies**: fmtlib is completely optional
- **Automatic Detection**: Uses preprocessor to detect available features
- **Backward Compatible**: Works with C++11, C++17, and C++20
- **Performance**: Modern formatting is more efficient than stringstream

## Implementation Details

### Files Modified

1. **example/server/format.hpp** (new)
   - Conditional compilation header
   - Detects std::format or fmtlib availability
   - Provides `detail::format()` wrapper function

2. **example/server/logger.hpp**
   - Added format string support when available
   - Maintains legacy variadic template API for fallback
   - Comprehensive inline documentation

3. **example/server/CMakeLists.txt**
   - Optional fmtlib detection via `find_package(fmt QUIET)`
   - Links fmtlib only if found
   - Status message when fmtlib is used

4. **example/client/burl/utils.cpp**
   - Updated `format_size()` to use format library
   - Resolved existing TODO comment
   - Same tiered approach as server example

5. **example/client/burl/CMakeLists.txt**
   - Optional fmtlib detection (same as server)

### Documentation Added

1. **example/server/README.md**
   - Complete usage guide
   - Build instructions with and without format support
   - Examples of both API styles

2. **example/server/logger_example.cpp**
   - Demonstrates both modern and legacy APIs
   - Shows conditional compilation in practice
   - Useful reference for users

3. **README.adoc**
   - Added Optional Dependencies section
   - Installation instructions for fmtlib
   - Clear explanation of auto-detection

## Usage Examples

### Modern Format String API (C++20 or with fmtlib)
```cpp
section log;
LOG_INF(log, "Request from {} for {}", client_ip, path);
LOG_DBG(log, "Response time: {:.2f}ms", response_time);
```

### Legacy Variadic API (C++11 fallback)
```cpp
section log;
LOG_INF(log, "Request from ", client_ip, " for ", path);
LOG_DBG(log, "Response time: ", response_time, "ms");
```

## Build Configurations

### With C++20 (auto-detects std::format)
```bash
cmake -DCMAKE_CXX_STANDARD=20 ..
make
```

### With fmtlib (any C++ standard)
```bash
# Using vcpkg
vcpkg install fmt
cmake -DCMAKE_TOOLCHAIN_FILE=path/to/vcpkg/scripts/buildsystems/vcpkg.cmake ..
make
```

### Without format support (C++11 compatible)
```bash
cmake ..
make
```

## Testing

All configurations tested successfully:
- ✅ C++11 without format library (stringstream fallback)
- ✅ C++20 with std::format (automatic detection)
- ✅ Backward compatibility (existing code works unchanged)

## Future Enhancements

Potential improvements (out of scope for this PR):
- Example demonstrating advanced format string features
- Performance benchmarks comparing stringstream vs format
- Integration with structured logging frameworks

## Technical Notes

### Why std::vformat instead of std::format?
`std::format` requires compile-time constant format strings in C++20. Since our logger needs to accept runtime format strings, we use `std::vformat` which accepts runtime strings.

### Why not Boost.Format?
While Boost.Format is available, it:
- Uses different syntax (`%1%` instead of `{}`)
- Is slower than modern alternatives
- Doesn't match the industry direction (std::format/fmtlib)

The chosen approach aligns with modern C++ best practices and industry standards.
