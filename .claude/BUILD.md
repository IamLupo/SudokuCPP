## Build Commands

### Release
```bash  
cmake -B build -S .
cmake --build build --config Release

# Run
./build/Release/hello.exe
```

### Debug
```bash  
cmake -B build -S .
cmake --build build --config Debug

# Run
./build/Debug/hello.exe
```