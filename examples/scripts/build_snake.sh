#!/bin/bash
# Build snake game

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EXAMPLES_DIR="$(dirname "$SCRIPT_DIR")"
PROJECT_DIR="$(dirname "$EXAMPLES_DIR")"

cd "$PROJECT_DIR"

echo "🐍 Building Snake Game..."
./build/olc examples/src/snake.olang -o examples/build/snake.o
./olang-link examples/build/snake examples/build/snake.o -lc -lncurses
echo "✅ Done! Run: ./examples/build/snake"

