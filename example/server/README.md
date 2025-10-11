# HTTP Server Example

This example demonstrates a basic HTTP server using Boost.Http.IO.

## Features

- Asynchronous request handling with Boost.Asio
- File serving with MIME type detection
- Custom logging system with format string support

## Format String Support

The logger supports modern C++ formatting when available:

### Using std::format (C++20)
When compiled with C++20, the logger automatically uses `std::format`:
```cpp
section log;
LOG_INF(log, "Received request from {} for {}", client_ip, path);
```

### Using fmtlib
If C++20 is not available but [fmtlib](https://github.com/fmtlib/fmt) is installed:
```bash
# Install fmtlib (example with vcpkg)
vcpkg install fmt

# Configure CMake to find fmtlib
cmake -DCMAKE_TOOLCHAIN_FILE=path/to/vcpkg/scripts/buildsystems/vcpkg.cmake ..
```

The logger will automatically detect and use fmtlib:
```cpp
section log;
LOG_INF(log, "Processing request {} of {}", count, total);
```

### Fallback (No Format Library)
If neither std::format nor fmtlib is available, the logger uses the legacy API:
```cpp
section log;
LOG_INF(log, "Processing request ", count, " of ", total);
```

## Building

### With Format Support (Recommended)
```bash
# Using C++20
cmake -DCMAKE_CXX_STANDARD=20 ..
make

# Or using fmtlib with C++11/17
cmake -DCMAKE_TOOLCHAIN_FILE=path/to/vcpkg/scripts/buildsystems/vcpkg.cmake ..
make
```

### Without Format Support
```bash
cmake ..
make
```

The server example will compile successfully with or without format library support.

## Usage

```bash
./http_io_server_example [document_root]
```

Where `document_root` is the directory to serve files from (defaults to current directory).
