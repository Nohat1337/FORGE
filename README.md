# Forge Programming Language

<p align="center">
  <img src="assets/forge-icon.png" alt="Forge Logo" width="200"/>
</p>

<p align="center">
  <strong>A modern, expressive programming language that compiles to bytecode and runs on a stack-based VM.</strong>
</p>

<p align="center">
  <a href="#features">Features</a> •
  <a href="#installation">Installation</a> •
  <a href="#quick-start">Quick Start</a> •
  <a href="#syntax-guide">Syntax Guide</a> •
  <a href="#standard-library">Standard Library</a> •
  <a href="#tools">Tools</a> •
  <a href="#contributing">Contributing</a>
</p>

---

## Features

| Feature | Description |
|---------|-------------|
| **Clean Syntax** | Familiar, readable, with modern ergonomics |
| **Bytecode VM** | Fast stack-based execution, JIT-ready architecture |
| **Compile to Bytecode** | `forgevm compile` produces `.fclass` binaries — load and run them with `forgevm` |
| **Rich Types** | Integers, floats, strings, arrays, maps, classes, closures |
| **Pattern Matching** | Powerful match expressions with guards and destructuring |
| **Generators** | `yield`-based coroutines with eager evaluation |
| **Modules** | Import system with stdlib (io, os, json, path, system) |
| **REPL** | Interactive mode with auto-print and history |
| **SDL2 IDE** | Full graphical IDE with editor, REPL, file browser, and debugger |
| **Package Manager** | `forge pkg install <name>` with community registry |
| **Tools** | Formatter, linter, debugger included |
| **Cross-Platform** | Linux (all distros), Windows, macOS |

---

## Installation

### Linux (Universal)

```bash
# download and run
wget https://github.com/forge-lang/forge/releases/latest/download/install_linux.sh
chmod +x install_linux.sh
./install_linux.sh
```

**Per-distro packages:**
| Distro | Command |
|--------|---------|
| Debian/Ubuntu | `sudo dpkg -i forge-lang_1.0.0_amd64.deb` |
| Fedora/RHEL | `sudo rpm -i forge-lang-1.0.0-1.x86_64.rpm` |
| Arch | `sudo pacman -U forge-lang-1.0.0-1-x86_64.pkg.tar.zst` |
| Alpine | `sudo apk add forge-lang-1.0.0-r1.apk` |
| openSUSE | `sudo zypper install forge-lang-1.0.0-1.x86_64.rpm` |

### Windows

Download the installer: [forge-1.0.0-windows-x64.exe](https://github.com/forge-lang/forge/releases/latest/download/forge-1.0.0-windows-x64.exe)

### macOS

```bash
brew tap forge-lang/forge
brew install forge
```

### From Source

```bash
git clone https://github.com/Nohat1337/FORGE.git
cd forge
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
sudo make install
```

---

## Quick Start

```bash
# Start REPL
forge

# Run a file
forge hello.fge

# Compile to bytecode
forgevm compile hello.fge hello.fclass

# Run compiled bytecode
forgevm hello.fclass

# Open SDL2 IDE
forge --ide

# Package manager
forge pkg update
forge pkg install stdlib
```

### Hello World

```forge
// hello.fge
print("Hello, Forge!")

// Variables
let name = "World"
const PI = 3.14159

// Functions
fn greet(person) {
    return "Hello, " + person + "!"
}

print(greet(name))

// Collections
let numbers = [1, 2, 3, 4, 5]
let user = { "name": "Alice", "age": 30, "active": true }

// Loops
for (let n in numbers) {
    print(n * 2)
}

// Match expression
let result = match numbers[0] {
    case 1: "one"
    case 2: "two"
    case _: "other"
}
```

---

## Syntax Guide

### Variables

```forge
let mutable = 10          // Mutable binding
const IMMUTABLE = 20      // Immutable (compile-time constant)
let x: int = 5            // Type annotation (optional, for documentation)
```

### Functions

```forge
fn add(a, b) {
    return a + b
}

// Implicit return (last expression)
fn multiply(a, b) {
    a * b
}

// Arrow functions (closures)
let double = fn(x) => x * 2

// Default parameters
fn greet(name = "World") {
    "Hello, " + name + "!"
}
```

### Control Flow

```forge
// If/else (expression)
let max = if (a > b) a else b

// Match expression
let description = match value {
    case 0: "zero"
    case 1: "one"
    case n if n > 10: "big"
    case [a, b]: "pair of " + str(a) + " and " + str(b)
    case { "error": msg }: "Error: " + msg
    case _: "unknown"
}

// Loops
for (let i = 0; i < 10; i = i + 1) { ... }
while (condition) { ... }
for (let item in collection) { ... }  // arrays, maps (keys), generators
```

### Classes & Inheritance

```forge
class Animal {
    fn init(name) {
        this.name = name
    }
    
    fn speak() {
        print(this.name + " makes a sound")
    }
}

class Dog extends Animal {
    fn init(name, breed) {
        super.init(name)
        this.breed = breed
    }
    
    fn speak() {
        print(this.name + " says: Woof! (" + this.breed + ")")
    }
}

let dog = Dog("Rex", "German Shepherd")
dog.speak()
```

### Generators

```forge
gen fibonacci() {
    let a = 0, b = 1
    while (true) {
        yield a
        let tmp = a
        a = b
        b = tmp + b
    }
}

for (let n in fibonacci()) {
    if (n > 100) break
    print(n)
}
```

### Error Handling

```forge
try {
    let result = risky_operation()
    print(result)
} catch (e) {
    print("Error: " + e)
} finally {
    cleanup()
}

// Throw custom errors
throw "Something went wrong"
throw { "code": 404, "message": "Not found" }
```

### Modules

```forge
import "io"
import "os" as system
import { read, write } from "io"

let content = io.read("file.txt")
system.execute("ls -la")
```

---

## Standard Library

### `io` — File I/O
```forge
io.read(path)           // Read entire file as string
io.write(path, content) // Write string to file
io.readLines(path)      // Read file as array of lines
io.exists(path)         // Check if file exists
io.println(path, line)  // Append line to file
```

### `os` — Operating System
```forge
os.execute(cmd)         // Run shell command, return output
os.env(key)             // Get environment variable
os.setEnv(key, value)   // Set environment variable
os.cwd()                // Current working directory
os.args()               // Command-line arguments array
os.time()               // Unix timestamp (ms)
os.sleep(ms)            // Sleep for milliseconds
```

### `json` — JSON Parsing
```forge
json.parse(str)         // Parse JSON string to Forge value
json.stringify(obj)     // Convert Forge value to JSON string
```

### `path` — Path Manipulation
```forge
path.join(...parts)     // Join path segments
path.base(path)         // Get filename
path.dir(path)          // Get directory
path.ext(path)          // Get extension
path.abs(path)          // Absolute path
```

### `system` — System Info
```forge
system.memory()         // Memory usage map
system.version()        // Forge version string
system.platform()       // "linux", "windows", "darwin"
```

### Built-in Functions
```forge
print(...args)          // Print to stdout
len(collection)         // Length of string/array/map
str(value)              // Convert to string
type(value)             // Type name as string
abs(n)                  // Absolute value
min(a, b), max(a, b)    // Min/max
sqrt(n), pow(a, b)      // Square root, power
floor(n), ceil(n)       // Floor/ceiling
round(n)                // Round to nearest
random()                // Random float 0-1
randomInt(min, max)     // Random integer in range
```

### String Methods
```forge
"hello".upper()         // "HELLO"
"HELLO".lower()         // "hello"
"  hi  ".trim()         // "hi"
"a,b,c".split(",")      // ["a", "b", "c"]
"hello".contains("ell") // true
"hello".replace("l", "x") // "hexxo"
"hello".substring(1, 4) // "ell"
"hello".charAt(0)       // "h"
"42".parseInt()         // 42
"3.14".toFloat()        // 3.14
```

### Map Functions
```forge
{ "a": 1 }.keys()       // ["a"]
{ "a": 1 }.values()     // [1]
{ "a": 1 }.entries()    // [["a", 1]]
{ "a": 1 }.has("a")     // true
{ "a": 1 }.clone()      // Copy
```

---

## Tools

### Formatter
```bash
forge-format file.fge           # Check formatting
forge-format --write file.fge   # Format in place
```

### Linter
```bash
forge-lint file.fge             # Lint single file
forge-lint src/                 # Lint directory
```

### Debugger
```bash
forge-debug file.fge                    # Run with debugger
forge-debug --breakpoint file.fge:10    # Break at line 10
forge-debug --step file.fge             # Step through
```

**Debug commands:** `s` (step), `n` (next), `c` (continue), `p expr` (print), `b file:line` (breakpoint), `w var` (watch), `l` (locals), `h` (help), `q` (quit)

### IDE
```bash
forge --ide                  # Open SDL2 IDE in current directory
forgevm --ide                # Same via forgevm
```

**Keybindings:**
- `Ctrl+S` — Save
- `Ctrl+N` — New file
- `Ctrl+O` — Open file
- `F5` — Toggle REPL
- `F9` — Run file
- `Ctrl+Z` / `Ctrl+Y` — Undo/Redo
- `Ctrl+Q` — Quit

---

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    Source (.fge)                        │
└──────────────────────┬──────────────────────────────────┘
                       ▼
┌─────────────────────────────────────────────────────────┐
│  Lexer ──► Parser ──► AST ──► Compiler ──► Bytecode     │
└──────────────────────┬──────────────────────────────────┘
                       ▼
┌─────────────────────────────────────────────────────────┐
│              Stack-Based Virtual Machine                │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐              │
│  │  Stack   │  │  Frames  │  │  Globals │              │
│  └──────────┘  └──────────┘  └──────────┘              │
└──────────────────────┬──────────────────────────────────┘
                       ▼
┌─────────────────────────────────────────────────────────┐
│        .fclass Bytecode ──► forgevm loader              │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐              │
│  │  CP      │  │ Methods  │  │  Code    │              │
│  └──────────┘  └──────────┘  └──────────┘              │
└─────────────────────────────────────────────────────────┘
```

- **OpCodes**: 60+ instructions for all language features
- **Memory**: Stack + heap with GC-ready object model
- **Calls**: Closure-based with upvalue capture
- **Types**: Tagged union (nil, bool, int, float, obj)
- **.fclass Format**: Binary bytecode with constant pool, methods, and code attributes

---

## Project Structure

```
forge/
├── src/                    # Core VM & Compiler
│   ├── main.cpp           # forge CLI entry point
│   ├── forgevm_main.cpp   # forgevm CLI entry point (compile/run)
│   ├── lexer.*            # Tokenizer
│   ├── parser.*           # Recursive descent parser
│   ├── compiler.*         # AST → Bytecode
│   ├── vm.*               # Original stack VM
│   ├── chunk.*            # Bytecode container
│   ├── value.*            # Value system
│   ├── ast.*              # AST nodes
│   ├── pkg_manager.cpp    # Package manager (forge pkg)
│   └── fvm/               # Forge Virtual Machine (FVM)
│       ├── runtime.*      # FVM interpreter + classfile loader
│       ├── classfile.*    # .fclass binary format (load/save)
│       ├── sdl2_ui.*      # SDL2 windowed UI primitives
│       ├── sdl_fvm_ui.cpp # Default FVM window (Forge logo)
│       └── forge_ide.*    # SDL2 IDE (editor, REPL, file browser)
├── tools/                 # Developer tools
│   ├── format.cpp         # Code formatter
│   ├── linter.cpp         # Static analysis
│   └── forge-debug.cpp    # Debugger
├── ide/                   # Legacy IDE directory (unused)
├── examples/              # Example programs
├── packages/              # Package registry (registry.json)
├── assets/                # Icons, logos
├── GUIDE.md               # Comprehensive user guide
├── .github/workflows/     # CI/CD pipeline
│   └── ci.yml
├── build_linux_package.sh # Linux packager
├── forge_installer.nsi    # Windows installer
└── CMakeLists.txt         # Build system
```

---

## Roadmap

- [x] **Bytecode Compiler** — Source → .fclass bytecode
- [x] **Forge VM (FVM)** — Production VM with GC, threading, FFI
- [x] **SDL2 UI** — Cross-platform windowed UI layer
- [x] **SDL2 IDE** — Full graphical IDE (editor, REPL, file browser)
- [x] **Package Manager** — `forge pkg` with community registry
- [x] **Compile & Run** — `forgevm compile` + `forgevm <file.fclass>`
- [ ] **JIT Compiler** — LLVM backend for hot paths
- [ ] **LSP Server** — Full IDE support (VS Code, Neovim)
- [ ] **WASM Target** — Run Forge in browsers
- [ ] **FFI** — Native C function calls
- [ ] **Gradual Typing** — Optional static type checking
- [ ] **Concurrency** — Actor model / channels

---

## Contributing

```bash
# 1. Fork & clone
git clone https://github.com/yourname/forge
cd forge

# 2. Create feature branch
git checkout -b feature/amazing-feature

# 3. Build & test
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
../forge ../examples/test_all.fge

# 4. Commit & push
git commit -m "feat: amazing feature"
git push origin feature/amazing-feature

# 5. Open PR
```

### Code Style
- 4-space indentation
- Clang-format (config in repo)
- Meaningful commit messages (conventional commits)

---

## License

MIT License — see [LICENSE](LICENSE) for details.

---

<p align="center">
  Made by nohat1337 - NH
</p>
<p align="center">
  <img src="assets/forge-icon.png" alt="Forge" width="100"/>
</p>
