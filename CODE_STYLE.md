# Forge Code Style Guide

## General
- Use 2-space indentation
- Semicolons optional but be consistent
- Prefer `let` for variables, `const` for immutable
- Functions declared with `fn`
- Classes use `class Name {}` with `init()` constructor

## Naming
- `camelCase` for variables and functions
- `PascalCase` for classes
- `SCREAMING_SNAKE_CASE` for constants

## Examples
```forge
fn greet(name) {
  print("Hello " + name)
}

class Person {
  init(name, age) {
    this.name = name
    this.age = age
  }
  
  introduce() {
    print(this.name)
  }
}
```

## FFI / Native
- Use `--trusted` flag when using `ffi.*`
- Always check return values
- Free allocated memory with `ffi.mem_free()`

## Android Liquid Glass
- Use `gui` module functions
- Call `gui.set_glass(id, blur, saturation, opacity)`
- Call `gui.set_tint(id, "#RRGGBB")` for tinted elements
