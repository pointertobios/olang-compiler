#!/bin/bash
# Build all examples

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

mkdir -p examples/build

echo "📦 Building all examples..."
echo ""

# Build snake
bash "$SCRIPT_DIR/build_snake.sh"

echo ""
echo "✅ All examples built successfully!"

