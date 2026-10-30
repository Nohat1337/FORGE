# LLMS.md — Guide for AI Agents Using Forge

This document tells AI models (and humans jumping in fast) exactly what works in Forge, what doesn't, and how to build/run. Everything in the "Verified Syntax" section has been executed against `./build-release/forgevm`. Treat anything marked ⚠️ as unusable until you've proven otherwise.

## Language Overview

Forge is a scripting language with C-like syntax compiled to bytecode and executed by the **FVM** (Forge Virtual Machine). Toolchain binaries live in `build-release/`.

- Run a script: `./build-release/forgevm script.fge`
- REPL: `./build-release/forgevm`
- One-liner: `./build-release/forgevm -e 'print(1+1)'`

## Verified Syntax (Working) ✅

```forge
// Variables
let x = 10
const name = "Forge"

// Functions
fn add(a, b) {
  return a + b
}

// Arrow functions (single-expression bodies)
let f = fn(x) => x * 2

// Nested functions & recursion work
fn outer() {
  fn fact(n) {
    if (n <= 1) { return 1 }
    return n * fact(n - 1)
  }
  return fact(5)
}

// Control flow
if (x > 5) { print("yes") } else { print("no") }
while (x > 0) { x = x - 1 }
for (let i = 0; i < 10; i = i + 1) { print(i) }
for (let item in [1, 2, 3]) { print(item) }
for (let k in keys({"a": 1})) { print(k) }

// Ternary
let big = x > 100 ? "big" : "small"

// Match — an EXPRESSION, usable inline
let label = match day {
  case 1: "Mon"
  case 2: "Tue"
  case _: "Weekend"
}
print(match day { case 3: "Wed" case _: "other" })

// Generators: gen/yield + next()
gen countTo(n) {
  let i = 1
  while (i <= n) {
    yield i
    i = i + 1
  }
}
let g = countTo(3)
print(next(g)) // 1
print(next(g)) // 2
print(next(g)) // 3
print(next(g)) // nil when exhausted

// Classes — methods MUST be declared with 'fn'
class Animal {
  fn init(name) { this.name = name }
  fn speak() { print("...") }
}
class Dog extends Animal {
  fn init(name) { super.init(name) }
  fn speak() { print("woof") }
}
let d = Dog("rex")
d.speak()

// Access specifiers (parsed, not enforced)
class Foo {
public:
  fn hi() { print("hi") }
private:
  fn helper() { print("internal") }
}

// Structs — fields space-separated; positional initialization
struct Point { x y }
let p = Point(3, 4)
print(p.x + p.y) // 7

// impl — add methods to a class/struct
impl Point {
  fn sum() { return this.x + this.y }
}
print(p.sum()) // 7

// Try/catch/throw
try {
  throw "boom"
} catch (e) {
  print(e)
}

// Arrays, maps, strings
let arr = [1, 2, 3]
let arr2 = arr + [4]            // concatenation
let sub = slice(arr2, 0, 2)
let m = {"key": "value"}
print(m["key"])
print(arr[1], len(arr2), contains("hello", "ell"))

// String interpolation
print("hello ${name}!")

// Deep equality — [1,2] == [1,2] is true
print([1, 2] == [1, 2])         // true
```

## NOT Supported (Avoid) ⚠️

- Default parameters: `fn f(a, b = 5)` → parse error
- If-expression: `let x = if (a) { b } else { c }` → parse error (use ternary)
- Method calls on literals: `"hello".upper()` → use `upper("hello")`
- `for (let kv in map)` → use `for (let k in keys(map))`
- Enum: `enum Color { RED, GREEN }` compiles but the variable is not bound
- Comma-separated struct fields: `struct P { x, y }` → use `struct P { x y }`

## Builtin Functions

```
print      len  push  array  next  str  int  float  type  error
keys    values  has  entries  slice  clone
upper   lower  trim  split  contains  replace  substring  charAt
parseInt   toFloat
abs   min  max  sqrt  pow  floor  ceil  round
random   randomInt
```

Generators: calling a `gen` function returns a generator object; `next(g)` drives it. `type(genval)` reports a generator as an object.

## Modules

Import with `import "name"`. Available modules:

| Module | Purpose | Notes |
| :-- | :-- | :-- |
| `io` | `io.read(path)`, `io.write(text[, "stdout"\|"stderr"\|file])` | |
| `os` | `os.time`, `os.args`, `os.execute`, `os.capture` | shell powers need `--trusted` |
| `json` | `json.stringify(v)`, `json.parse(str)` | real parser; depth limit 128 |
| `path` | `path.join(a, b)`, `path.base(p)` | |
| `system` | system info | |
| `fs` | `fs.read_file`, `fs.write_file`, `fs.remove`, `fs.rename`, `fs.create_dir`, `fs.exists` | |
| `test` | `test.describe`, `test.assert`, `test.assertEquals`, `test.results` | see `tests/test_core.fge` |
| `ffi` | `ffi.open`, `ffi.sym`, `ffi.call_*`, memory ops | every call requires `--trusted` |
| `gui` | Liquid Glass widgets | Android + desktop preview |
| `llm` | GGUF loading, tokenize, generate, NH-2-Coder | see `tests/test_gguf.fge` |
| `concurrent` | Actors + channels | |
| `compute` | accelerator context | |
| `sdl` | windowed UI backend | via `forgevm --sdl` |

## CLI Usage

```bash
./build-release/forgevm script.fge            # Run (also Accepts .fclass)
./build-release/forgevm --trusted script.fge  # FFI + os shell powers
./build-release/forgevm -e 'print("hi")'      # Execute inline
./build-release/forgevm                       # REPL
./build-release/forgevm compile app.fge out.fclass
./build-release/forgevm build                 # Forgefile single-file build (legacy ForgeLists.txt still accepted)
./build-release/forgevm build --shared        # C-ABI shared library export
./build-release/forgevm lib new mylib --cpp   # Native C++ lib scaffold
./build-release/forgevm java app.fge          # .fge -> Java / .jar
./build-release/forgevm android new|build|install|run
./build-release/forgevm --sdl script.fge      # SDL2 windowed UI
```

### Building projects: the Forgefile (ONE file for ONE binary, sessions built in)

`forgevm build` reads a single hand-written project file — components are listed by hand (no globbing). The file REALIZES which session to run via `session(...)`:

```forge
# Forgefile / ForgeLists.txt - compile session (default)
project(MyApp)
output(MyApp)
component(src/lib.fge)          # order matters: dependencies first
component(src/main.fge)
module(io)
libsdir(/usr/forge/libs)
uselib(mylib.so)
builddir(build-releases)
```

```forge
# session(decompile): one binary -> REAL source files + generated project + compile chain
project(decompile_rt)
session(decompile)
input(rtapp)                    # the single binary (or .fclass)
destination(decomp)             # decompiled sources + generated ForgeLists.txt go here
# result: decomp/src/*.fge (exact), decomp/ForgeLists.txt,
#        decomp/build-releases/rtapp (auto-compiled, add --no-build to skip)
```

```forge
# session(c2forge): C/C++ -> .fge -> generated project -> compile chain
session(c2forge)
input(main.c)
destination(cout)

# session(forge2c): .fge -> C files -> destination (+ generated doc lists)
session(forge2c)
input(src/lib.fge)
destination(cexport)
```

Console painting convention: `::` phases, `=>` checks, `-- text! -- {time}` done lines, `==> launch` lines, `[ NN%] from -> to` progress. Build commands with full args go to `build-releases/make.ts` and `cache/config.ts` (hidden unless `-v`).

`cfmake configure|build|init|clean` automates this: it detects the forge binaries, scans libs, generates a prefilled `Forgefile` when absent, maintains `build-releases/cache/`, then runs `forgevm build` automatically. `c2forge`/`forge2c` register translated components into the Forgefile; `forge-fclass-decompiler` emits one from decompiled sources.

Every forge-built single binary embeds its exact original sources (marker-fenced, `// FORGE-BUILD-METADATA: file=...`), so `session(decompile)` recovers them bit-exact. Components must be listed in dependency order — a component's functions are visible only after it loads.

## Security Model (important for agents)

- **Without `--trusted`,** `ffi.*`, `os.execute`, and `os.capture` throw a runtime error telling you to pass `--trusted`. Don't "work around" it — ask the user.
- Type mismatches (e.g. `os.execute(42)`) raise `Type error: expected string` instead of crashing.
- `fs`/`io` can read/write anywhere the OS user can. Only run code you trust — same rules as Python.

## Error Messages

Runtime errors look like:

```
Runtime Error: Division by zero
  in divide (line 3)
    called from (script) (line 12)
```

Parser errors: `Error: Expected ')' at line 1:11 (got '=')`. On any error, re-read the message before rewriting code.

## FFI Example

```forge
import "ffi"
let lib = ffi.open("./mylib.so")     // .so / .dylib / .dll
let add = ffi.sym(lib, "add")
print(ffi.call_int2(add, 2, 3))      // 5
ffi.close(lib)
```

Only in `--trusted` mode. Scaffold a matching library with `forgevm lib new mylib --cpp`.

## Android Liquid Glass

```forge
import "gui"
gui.create_card("card1", 10, 10, 300, 200)
gui.set_glass("card1", 0.8, 1.2, 0.9)   // blur, saturation, opacity
gui.set_tint("card1", "#80FF6B35")      // ARGB with alpha
gui.create_button("btn1", "Click Me", 50, 150)
gui.on_click("btn1", fn() {
  print("Button clicked!")
})
```

Widgets: `create_bottombar`, `create_segmented`, `create_card`, `create_stepper`, `create_magnifier`, `create_search`, `create_draggertabs`. Android build pins Kyant0 backdrop 1.0.6.

## Testing

```bash
./build-release/forgevm tests/test_core.fge     # the big one (110 checks)
./build-release/forgevm tests/test_minimal.fge
python3 tools/lsp_smoke.py                       # LSP checks
```

The `test` module pattern:

```forge
import "test"
test.describe("Section")
test.assertEquals(1 + 1, 2, "arithmetic")
test.results()
```

## Tips for AI Coding

1. Write only what appears in Verified Syntax. If you must use something unverified, test it in a throwaway file first.
2. Rebuild with `cmake --build build-release -j4` after changing sources; binaries don't auto-rebuild.
3. Run tests after any runtime/compiler change: `test_core` exercises most builtins.
4. Avoid `--trusted` unless the task genuinely needs native interop or shell.
5. Bug reports to yourself: a silent `nil` usually means a stubbed function or an unverified construct, not bad luck.
