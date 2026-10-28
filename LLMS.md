# LLMS.md - Guide for AI Agents Using Forge

This document helps AI models understand and write Forge code effectively.

## Language Overview
Forge is a C-like scripting language compiled to bytecode for the FVM (Forge Virtual Machine).

## Verified Syntax (Working)
```forge
// Variables
let x = 10
const name = "Forge"

// Functions
fn add(a, b) {
  return a + b
}

// Closures
let f = fn(x) { return x * 2 }

// Control flow
if (x > 5) { print("yes") } else { print("no") }
while (x > 0) { x = x - 1 }
for (let i = 0; i < 10; i = i + 1) { print(i) }
for (let item in array) { print(item) }

// Match
match (x) {
  case 1: print("one")
  case 2: print("two")
  case _: print("other")
}

// Classes
class Animal {
  init(name) { this.name = name }
  speak() { print("...") }
}
class Dog extends Animal {
  speak() { print("woof") }
}

// Try/Catch
try {
  throw "error"
} catch (e) {
  print(e)
}

// Arrays & Maps
let arr = [1, 2, 3]
let map = {"key": "value"}
```

## NOT Supported (Avoid)
- Arrow functions: `fn(x) => x * 2` 
- Default parameters
- Ternary/expression-if: `let x = if (a) b else c`
- Generators: `gen/yield`
- Method calls on string literals: `"hello".upper()`
- `for (k,v) in map` (map iteration has issues)

## Modules
- `gui` - UI components (Android + desktop)
- `io` - File I/O
- `os` - OS operations
- `system` - System calls
- `json` - JSON (parse is stub, use carefully)
- `path` - Path utilities
- `ffi` - Native interop (requires `--trusted`)

## CLI Usage
```bash
forgevm script.fge           # Run
forgevm --trusted script.fge # With FFI
forgevm build                # Build project
forgevm build --shared       # Export shared library
forgevm lib new name --cpp   # Create C++ native lib
forgevm android build ./app  # Build Android
```

## Android Liquid Glass Integration
```forge
// Initialize glass UI
gui.create_card("card1", 10, 10, 300, 200)
gui.set_glass("card1", 0.8, 1.2, 0.9)  // blur, saturation, opacity
gui.set_tint("card1", "#80FF6B35")     // tinted with alpha

gui.create_button("btn1", "Click Me", 50, 150)
gui.on_click("btn1", fn() {
  print("Button clicked!")
})
```

## FFI Example
```forge
// With --trusted
let lib = ffi.open("./mylib.so")
let add = ffi.sym(lib, "add")
let result = ffi.call_int2(add, 2, 3)
ffi.close(lib)
```

## Tips for AI Coding
1. Always check if feature is in "Verified Syntax" section
2. Use `forgevm --trusted` only when needed
3. For Android UI use gui module functions
4. Test with `./build-release/forgevm file.fge`
5. Build with `cmake --build build-release --target forgevm -j4`
