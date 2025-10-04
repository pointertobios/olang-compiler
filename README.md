# Olang Compiler

LLVM-based programming language compiler

## Build

```bash
mkdir build && cd build
cmake .. && make -j$(nproc)
```

## Snake Game Example

```bash
cd build
./olc ../examples/tui_app.olang -o ../examples/build/tui_app.o
cd ..
./olang-link ./examples/build/tui_app ./examples/build/tui_app.o -lncurses

# For better experience with colors:
export TERM=xterm-256color
./examples/build/tui_app
```

**Controls**: ↑↓←→ arrow keys to move, q to quit

## Compiler (olc)

```bash
./olc <file.olang>              # Generate .o
./olc <file.olang> --emit-llvm  # Generate .ll
./olc <file.olang> --print-ir   # Print IR
```

## Linker

```bash
./olang-link <output> <input.o> [-lncurses ...]
```

## Language Features

- Basic types: i1, i8, i16, i32, i64, f32, f64
- Structs and arrays
- Functions: internal, extern declarations, export
- Control flow: if/else, while
- Operators: arithmetic, comparison, logical
- Pointers

## Dependencies

- C++17
- CMake 3.20+
- LLVM 18
- Java (ANTLR)
- ld.lld
