# Forge Programming Language — User Guide

## Table of Contents
1. [Installation](#installation)
2. [Quick Start](#quick-start)
3. [Forge Language](#forge-language)
4. [SDL2 GUI Programming](#sdl2-gui-programming)
5. [Package Manager](#package-manager)
6. [Building Projects](#building-projects)
7. [Running .fclass Bytecode](#running-fclass-bytecode)
8. [Forge IDE](#forge-ide)
9. [Examples](#examples)

---

## Installation

### Building from Source

**Requirements:**
- C++17 compiler (g++ or clang++)
- SDL2 development libraries
- curl (for package manager)
- CMake 3.14+

```bash
# Install dependencies (Ubuntu/Debian)
sudo apt install build-essential cmake libsdl2-dev curl

# Clone and build
git clone https://github.com/Nohat1337/FORGE.git
cd FORGE
mkdir build && cd build
cmake .. && make -j$(nproc)

# Install system-wide (optional)
sudo cp forge /usr/local/bin/
sudo cp forgevm /usr/local/bin/
```

### Verify Installation

```bash
forge --version        # Forge Programming Language v1.0.0
forgevm --version      # Forge VM v2.0.0 (FVM)
forge --help           # Show all commands
```

---

## Quick Start

### Hello World

Create `hello.fge`:
```forge
print("Hello, Forge!")
```

Run it:
```bash
forge hello.fge
```

### Using the REPL

```bash
>> print("Hello!")
Hello!
>> var x = 42
>> print(x * 2)
84
>> exit
```

### Command-Line Execution

```bash
# Run a file
forge myfile.fge

# Execute code directly
forge -e 'print(2 + 2)'

# Run compiled bytecode
forge myfile.fclass
```

---

## Forge Language

### Variables

```forge
var name = "Forge"        # mutable variable
val pi = 3.14159          # immutable constant
const MAX = 1000          # compile-time constant
```

### Data Types

```forge
# Primitives
var i = 42                # integer
var f = 3.14              # float
var s = "hello"           # string
var b = true              # boolean
var n = null              # nil

# Collections
var arr = [1, 2, 3]       # array
var map = { "key": "value" }  # map
```

### Functions

```forge
fn greet(name) {
    print("Hello, " + name + "!")
}

fn add(a, b) {
    return a + b
}

# Arrow syntax for simple functions
fn square(x) = x * x
```

### Control Flow

```forge
# If/else
if (x > 10) {
    print("big")
} else {
    print("small")
}

# While loops
var i = 0
while (i < 10) {
    print(i)
    i = i + 1
}

# For loops
for (var i = 0; i < 10; i = i + 1) {
    print(i)
}

# Break and continue
for (var i = 0; i < 100; i = i + 1) {
    if (i == 5) continue
    if (i == 10) break
    print(i)
}
```

### Classes

```forge
class Dog {
    var name = ""
    var breed = ""

    fn init(name, breed) {
        this.name = name
        this.breed = breed
    }

    fn bark() {
        print(this.name + " says: Woof!")
    }
}

var rex = new Dog("Rex", "German Shepherd")
rex.bark()
```

### Inheritance

```forge
class Animal {
    var name = ""
    fn speak() { print("...") }
}

class Cat extends Animal {
    fn speak() {
        print(this.name + " says: Meow!")
    }
}
```

### Error Handling

```forge
try {
    var result = riskyOperation()
} catch (e) {
    print("Error: " + str(e))
}
```

### Imports

```forge
import "io"
import "os"
import "json"
import "path"
```

---

## SDL2 GUI Programming

### Creating a Window

```forge
import "sdl"

sdl.init(800, 600, "My Forge App")
```

### Drawing Primitives

```forge
import "sdl"

sdl.init(800, 600, "Drawing Demo")

# Clear screen
sdl.clear(30, 30, 46)

# Draw shapes
sdl.rect(100, 100, 200, 100, 137, 180, 250)       # filled rectangle
sdl.rect_outline(100, 100, 200, 100, 255, 255, 255) # outline
sdl.line(100, 100, 300, 200, 255, 100, 100)        # line
sdl.circle(400, 300, 50, 166, 227, 161)            # filled circle

# Draw text
sdl.text(100, 50, "Hello Forge!", 205, 214, 244)
sdl.text_bg(100, 350, "Highlighted", 255, 255, 255, 50, 50, 70)

# Rounded rectangle
sdl.rounded_rect(100, 400, 200, 80, 137, 180, 250, 8)

# Gradient
sdl.gradient(100, 500, 300, 50, 137, 180, 250, 166, 227, 161)

# Shadow
sdl.shadow(100, 400, 200, 80, 8, 60)

# Update screen
sdl.present()
sdl.delay(3000)
sdl.quit()
```

### Event Loop

```forge
import "sdl"

sdl.init(800, 600, "Event Demo")

var running = true
while (running) {
    sdl.clear(30, 30, 46)

    # Draw UI
    sdl.text(10, 10, "Press ESC or click X to quit", 205, 214, 244)
    sdl.present()

    # Poll events
    var event = sdl.poll()
    if (event.type == "quit") {
        running = false
    } else if (event.type == "key") {
        if (event.key == 27) {  # ESC
            running = false
        }
    } else if (event.type == "click") {
        print("Clicked at: " + str(event.x) + ", " + str(event.y))
    }

    sdl.delay(16)
}

sdl.quit()
```

### Buttons

```forge
import "sdl"

sdl.init(800, 600, "Button Demo")
var mouseX = 0
var mouseY = 0
var mouseDown = false
var running = true

while (running) {
    sdl.clear(30, 30, 46)

    # Draw button
    var clicked = sdl.button(100, 100, 150, 40, "Click Me!", 137, 180, 250, mouseX, mouseY, mouseDown)
    if (clicked) {
        print("Button clicked!")
    }

    sdl.present()

    var event = sdl.poll()
    if (event.type == "quit") running = false
    if (event.type == "motion" || event.type == "click") {
        mouseX = event.x
        mouseY = event.y
        mouseDown = event.pressed
    }

    sdl.delay(16)
}
sdl.quit()
```

### Clickable Links

```forge
import "sdl"

sdl.init(800, 200, "Link Demo")
var mouseX = 0
var mouseY = 0
var mouseDown = false
var running = true

while (running) {
    sdl.clear(30, 30, 46)
    sdl.text(20, 20, "Visit:", 205, 214, 244)
    sdl.link(100, 20, "Forge Website", "https://github.com/Nohat1337/FORGE", 137, 180, 250, mouseX, mouseY, mouseDown)
    sdl.present()

    var event = sdl.poll()
    if (event.type == "quit") running = false
    if (event.type == "motion" || event.type == "click") {
        mouseX = event.x
        mouseY = event.y
        mouseDown = event.pressed
    }
    sdl.delay(16)
}
sdl.quit()
```

### Clipboard

```forge
import "sdl"

sdl.init(800, 600, "Clipboard Demo")

# Set clipboard
sdl.clipboard_set("Hello from Forge!")

# Get clipboard
var text = sdl.clipboard_get()
print("Clipboard: " + text)

sdl.quit()
```

### Text Selection

```forge
import "sdl"

sdl.init(800, 600, "Text Selection")
sdl.clear(30, 30, 46)

# Draw text with selection highlight
sdl.text_selection(100, 100, 600, 20,
    "This is selectable text",
    5, 10,                     # selection start/end
    255, 255, 255,             # text color
    30, 30, 46,                # background
    70, 130, 230)              # selection color

sdl.present()
sdl.delay(3000)
sdl.quit()
```

### Full SDL2 API Reference

| Function | Arguments | Returns | Description |
|---|---|---|---|
| `sdl.init` | `w, h, title` | `bool` | Create window |
| `sdl.quit` | — | `nil` | Destroy window |
| `sdl.clear` | `r, g, b` | `nil` | Clear screen |
| `sdl.present` | — | `nil` | Swap buffers |
| `sdl.delay` | `ms` | `nil` | Sleep |
| `sdl.ticks` | — | `int` | Milliseconds |
| `sdl.rect` | `x, y, w, h, r, g, b[, a]` | `nil` | Filled rectangle |
| `sdl.rect_outline` | `x, y, w, h, r, g, b` | `nil` | Rectangle outline |
| `sdl.line` | `x0, y0, x1, y1, r, g, b` | `nil` | Draw line |
| `sdl.circle` | `cx, cy, radius, r, g, b` | `nil` | Filled circle |
| `sdl.text` | `x, y, text, r, g, b` | `nil` | Draw text |
| `sdl.text_bg` | `x, y, text, fg_r, fg_g, fg_b, bg_r, bg_g, bg_b` | `nil` | Text with bg |
| `sdl.text_width` | `text` | `int` | Pixel width |
| `sdl.text_height` | — | `int` | Pixel height (8) |
| `sdl.size` | — | `[w, h]` | Window size |
| `sdl.poll` | — | `map` | Event map |
| `sdl.screenshot` | `path` | `bool` | Save BMP |
| `sdl.button` | `x,y,w,h,label,r,g,b,mx,my,mpressed` | `bool` | Button |
| `sdl.link` | `x,y,text,url,r,g,b,mx,my,mpressed` | `bool` | Hyperlink |
| `sdl.clipboard_get` | — | `string` | Get clipboard |
| `sdl.clipboard_set` | `text` | `nil` | Set clipboard |
| `sdl.text_selection` | `x,y,w,h,text,sel_s,sel_e,fg_r,fg_g,fg_b,bg_r,bg_g,bg_b,sel_r,sel_g,sel_b` | `nil` | Selectable text |
| `sdl.rounded_rect` | `x,y,w,h,r,g,b,radius` | `nil` | Rounded rectangle |
| `sdl.rounded_rect_outline` | `x,y,w,h,r,g,b,radius` | `nil` | Rounded outline |
| `sdl.gradient` | `x,y,w,h,r1,g1,b1,r2,g2,b2` | `nil` | Vertical gradient |
| `sdl.shadow` | `x,y,w,h,radius,alpha` | `nil` | Drop shadow |

### Event Types

| Type | Fields | Description |
|---|---|---|
| `"quit"` | — | Window closed |
| `"key"` | `key, char, shift, ctrl, alt` | Key pressed |
| `"click"` | `x, y, button, pressed, released` | Mouse click |
| `"motion"` | `x, y` | Mouse moved |
| `"resize"` | `w, h` | Window resized |
| `"none"` | — | No event |

---

## Package Manager

### Search Packages

```bash
forge pkg search stdlib
forge pkg search ui
forge pkg search sdl
```

### Install Packages

```bash
forge pkg install stdlib       # Install stdlib
forge pkg install ui@1.0.0     # Install specific version
forge pkg install              # Install all from forge.json
```

### Manage Packages

```bash
forge pkg list                 # List installed packages
forge pkg remove stdlib        # Remove a package
forge pkg update               # Refresh registry cache
forge pkg cache clean          # Clean download cache
```

### Create a Project

```bash
forge pkg init my-project 1.0.0
# Creates: forge.json, src/, lib/, tests/
```

### Add Dependencies

```bash
forge pkg add stdlib
forge pkg add --dev test-runner
```

### forge.json Example

```json
{
  "name": "my-project",
  "version": "1.0.0",
  "description": "My Forge project",
  "author": "username",
  "license": "MIT",
  "main": "main.fge",
  "dependencies": {
    "stdlib": "1.0.0"
  },
  "devDependencies": {}
}
```

---

## Building Projects

### Project Structure

```
my-project/
├── forge.json          # Project manifest
├── src/
│   └── main.fge        # Entry point
├── lib/                # Libraries
├── tests/              # Tests
└── ForgeLists.txt      # Build targets (optional)
```

### Build to .fclass Bytecode

```bash
# Compile .fge to .fclass
forgevm compile src/main.fge

# Compile with custom output name
forgevm compile src/main.fge output/myapp.fclass

# Run compiled bytecode (fast!)
forge output/myapp.fclass
```

### Build Standalone Binary

```bash
forgevm build
# Produces: build/my-project
```

---

## Running .fclass Bytecode

The `.fclass` format is Forge's compiled bytecode format, analogous to Java's `.class` files.

### Compile and Run

```bash
# Step 1: Compile source to bytecode
forgevm compile app.fge

# Step 2: Run the compiled bytecode
forge app.fclass
```

### Auto-Detection

Both `forge` and `forgevm` auto-detect file extensions:
- `.fge` files → compiled from source, then executed
- `.fclass` files → loaded as bytecode, executed directly

```bash
forge myfile.fge       # Compiles and runs source
forge myfile.fclass    # Runs pre-compiled bytecode
```

### .fclass Binary Format

```
Magic: 0xCAFEF00D
Version: 1.0
Constant Pool: UTF8, INTEGER, FLOAT, LONG, DOUBLE, CLASS, STRING, METHODREF, NAME_AND_TYPE
Methods: name, descriptor, bytecode, maxStack, maxLocals, exceptionTable
Attributes: CODE, SOURCE_FILE, LINE_NUMBER_TABLE
```

---

## Forge IDE

Launch the full SDL2-based development environment:

```bash
forge --ide
```

### Features

- **File Explorer** — Browse and open files from the left panel
- **Code Editor** — Syntax highlighting with cursor navigation
- **Tab System** — Multiple open files with tabs
- **Toolbar** — New, Open, Save, Run, Build buttons
- **Terminal** — Built-in terminal for commands
- **Status Bar** — File info, line/column, modification status

### Keyboard Shortcuts

| Key | Action |
|---|---|
| `Ctrl+S` | Save current file |
| `Ctrl+C` | Copy line |
| `Ctrl+V` | Paste |
| `Ctrl+X` | Cut line |
| `Ctrl+A` | Select all |
| `Arrow Keys` | Navigate |
| `Home/End` | Start/end of line |
| `Tab` | Insert 4 spaces |
| `Backspace` | Delete backward |
| `Delete` | Delete forward |
| `Enter` | New line |

---

## Examples

### Hello World
```forge
print("Hello, Forge!")
```

### FizzBuzz
```forge
for (var i = 1; i <= 100; i = i + 1) {
    if (i % 15 == 0) print("FizzBuzz")
    else if (i % 3 == 0) print("Fizz")
    else if (i % 5 == 0) print("Buzz")
    else print(str(i))
}
```

### SDL2 Window with Button
```forge
import "sdl"

sdl.init(800, 600, "Forge App")
var mx = 0
var my = 0
var md = false
var count = 0
var running = true

while (running) {
    sdl.clear(30, 30, 46)
    sdl.text(10, 10, "Button clicks: " + str(count), 205, 214, 244)

    if (sdl.button(100, 100, 150, 40, "Click Me", 137, 180, 250, mx, my, md)) {
        count = count + 1
    }

    sdl.present()

    var e = sdl.poll()
    if (e.type == "quit") running = false
    if (e.type == "motion" || e.type == "click") {
        mx = e.x
        my = e.y
        md = e.pressed
    }
    sdl.delay(16)
}
sdl.quit()
```

### Class Example
```forge
class Vector {
    var x = 0
    var y = 0

    fn init(x, y) {
        this.x = x
        this.y = y
    }

    fn add(other) {
        return new Vector(this.x + other.x, this.y + other.y)
    }

    fn length() {
        return sqrt(this.x * this.x + this.y * this.y)
    }

    fn toString() {
        return "Vector(" + str(this.x) + ", " + str(this.y) + ")"
    }
}

var v1 = new Vector(3, 4)
var v2 = new Vector(1, 2)
var v3 = v1.add(v2)
print(v3.toString())
print("Length: " + str(v1.length()))
```

---

## Built-in Modules

| Module | Functions |
|---|---|
| `io` | `write`, `read` |
| `os` | `time`, `execute`, `capture`, `args` |
| `fs` | `read_dir`, `is_dir`, `exists`, `read_file`, `write_file`, `remove`, `rename`, `create_dir` |
| `json` | `parse`, `stringify` |
| `path` | `join`, `exists`, `is_file`, `is_dir`, `read_dir` |
| `system` | `env`, `exit`, `clock`, `sleep` |
| `ui` | `init`, `cleanup`, `clear`, `draw_text`, `draw_rect`, `get_size`, `read_key` |
| `sdl` | (see [SDL2 GUI Programming](#sdl2-gui-programming)) |
| `test` | `assert`, `assertEquals`, `assertNotEquals`, `describe`, `results` |

### Built-in Functions

| Function | Description |
|---|---|
| `print(...)` | Print to stdout |
| `len(x)` | Length of string/array |
| `str(x)` | Convert to string |
| `int(x)` | Convert to integer |
| `float(x)` | Convert to float |
| `type(x)` | Get type name |
| `error(msg)` | Throw an error |
| `push(arr, x)` | Append to array |
| `keys(map)` | Get map keys |
| `values(map)` | Get map values |
| `has(map, key)` | Check key exists |
| `entries(map)` | Get key-value pairs |
| `clone(x)` | Deep clone |
| `upper(s)` | Uppercase string |
| `lower(s)` | Lowercase string |
| `trim(s)` | Trim whitespace |
| `split(s, d)` | Split string |
| `contains(s, sub)` | Check substring |
| `replace(s, old, new)` | Replace in string |
| `substring(s, start, end)` | Extract substring |
| `charAt(s, i)` | Get character at index |
| `parseInt(s)` | Parse integer |
| `abs(x)` | Absolute value |
| `min(a, b)` | Minimum |
| `max(a, b)` | Maximum |
| `sqrt(x)` | Square root |
| `pow(x, y)` | Power |
| `floor(x)` | Floor |
| `ceil(x)` | Ceil |
| `round(x)` | Round |
| `random()` | Random 0-1 |
| `randomInt(n)` | Random 0-n |

---

## License

Forge Programming Language — MIT License
