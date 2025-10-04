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
./olc ../examples/src/snake.olang -o ../examples/build/snake.o
cd ..
./olang-link ./examples/build/snake ./examples/build/snake.o -lncurses -lc

./example/build/snake
```

**Controls**: ↑↓←→ arrow keys to move, q to quit

## Compiler (olc)

```bash
Usage: ./build/olc <input_file> [options]
Options:
  --emit-llvm       Generate LLVM IR (.ll)
  -o <output>       Specify output file
  --target <triple> Specify target triple
  --print-ir        Print LLVM IR to stdout

Default: Generate object file (.o)
Linking: Use ld.lld or clang to link .o files
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
