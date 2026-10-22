# Brag Plan: Forge Programming Language

## What is this app?
Forge is a programming language implemented in C++ that compiles to bytecode (`.fclass`) and runs on a stack-based VM — with an SDL2 IDE, a package manager, native C interop, and an `llm` module that loads a real GGUF model (SmolLM-135M) and runs inference **inside the language runtime**.

## The angle
Almost every programming-language demo shows syntax. This one shows the language *doing the thing*. The video lives inside the product's own UI: a terminal in the Forge VS Code Dark+ palette types `import "llm"` and the runtime prints real model-loader logs. The hook is not "look, a language" — it is "wait, that language just loaded a neural network." Everything after that is proof it is a real toolchain: an IDE, a compiler that emits bytecode, native C interop, and an agent whose tools run in-process.

Every line of terminal output in this video is verbatim program output captured from the built binaries in this repository. Nothing is invented.

## Hook (first 2-3 seconds)
Frame 0 already carries the claim, in the product's own type: **"a programming language with a 135M model inside it"** (accent-tinted `135M`). Under it, the Forge terminal window opens and types `import "llm"` character by character, then streams the real loader output:

```
[LLAMA] Loading: models/SmolLM-135M.Q8_0.gguf
[LLAMA] Vocab=49152 Context=2048
[LLAMA] Ready!
```

## Key moments (the middle)
- **The IDE** — recreated in the product's exact VS Code Dark+ palette (`#1E1E1E` editor, `#252526` panels, `#007ACC` accent): activity bar, `EXPLORER` sidebar, tab bar, syntax-highlighted Forge source, a `TERMINAL` panel printing `Hello, Forge!`, the blue gradient status bar (`Ln 1, Col 1    Spaces: 4    Forge    UTF-8`), and the IDE's own welcome tagline `write. compile. run.`
- **The bytecode pipeline** — `forgevm compile hello.fge hello.fclass` → `Compiled hello.fge -> hello.fclass` / `  135 bytes, 1 method(s)`, then `forgevm hello.fclass` → `Hello, Forge!`
- **Native interop** — a Forge script calling C through `ffi.open` / `ffi.sym` / `ffi.call_int` → `add(2, 3)          = 5`, `fibonacci(20)      = 6765`
- **The agent** — `llm.faw_tool("calculate", "expression", "2 + 3 * 7 - 4 / 2")` → `21`: the FAW agent's tools execute in-process, inside the VM

## Outro / punchline
The tool call resolves to `21`, then the end card: the FORGE wordmark, the README's own claim — *"Compiles to bytecode. Runs on a stack-based VM."* — and the project URL `github.com/Nohat1337/FORGE`.

## User flow worth showing
Entry → key action → result, shown as the terminal actually runs it:
1. **Entry** — `$ forge hello.fge` → `Hello, Forge!`
2. **Key action** — `$ forgevm compile hello.fge hello.fclass` → `Compiled hello.fge -> hello.fclass` + `  135 bytes, 1 method(s)`
3. **Result** — `$ forgevm hello.fclass` → `Hello, Forge!` (bytecode runs on the VM, not the source path)

The centerpiece scene is this live pipeline in the terminal, not a feature list.

## Tone
- Preset: `default`
- Creative direction: confident developer-tool launch — the terminal is the hero, the IDE is the proof, no hype adjectives, real output on screen
- Interpretation: playful but factual. Fast entrances, slow holds, every string legible. The camera never leaves the product's own UI, so the video reads as a screencast of something real rather than a marketing render.

## Format: landscape — 1920x1080
## Duration: 20.02 seconds (five 4-second scenes, boundaries landing on the 120 BPM beat grid)

## Visual identity (from the project)
- Background: `#1E1E1E` (IDE editor surface)
- Panel: `#252526` (sidebar, tab bar, terminal panel) · Activity bar `#333333` · Title bar `#373738`
- Accent / brand: `#007ACC` (status bar top, primary accent) — gradient to `#005690` at the status bar base
- Text: `#D4D4D4` foreground · dim `#858585` line numbers · bright `#E7E7E7` directory names
- Syntax colors (VS Code Dark+, verbatim from `src/fvm/forge_ide.cpp`): keyword `#569CD6`, control `#C586C0`, type `#4EC9B0`, function `#DCDCAA`, string `#CE9178`, number `#B5CEA8`, comment `#6A9955`
- Display font: DejaVu Sans Bold (local `@font-face`) — the IDE wordmark is an 8×8 bitmap face, so the HTML wordmark is a spaced, tracked uppercase lockup in the same accent
- Body / terminal font: JetBrains Mono (local `@font-face`)
- Strongest visual element: the Forge IDE chrome itself — activity bar, `EXPLORER` tree, tab bar, blue gradient status bar, terminal panel

## Share copy (draft)
I wrote a programming language and put a 135M-parameter model inside it. `import "llm"` is all it takes.

## Audio direction
- Role: warm bed + motion-matched UI accents
- Music: `happy-beats-business-moves-vol-1-by-ende-dot-app.mp3` (120.19 BPM, 2:44)
- Music treatment: starts at 0.0 under the headline, volume 0.32, fades to 0 under the end card's final beat (last 1.2s)
- Music cue guidance: bundled preset `assets/music/cues/happy-beats-business-moves-vol-1-by-ende-dot-app.music-cues.json`. Scene boundaries are locked to the beat grid at 4.02 / 8.02 / 12.02 / 16.02 / 20.02; 16.02 and 20.02 are strong cues (`// beat-locked`). Small entrances inside scenes snap to the half-beat grid (x.02 / x.52). Sequential terminal lines are revealed quickly and held together, never one per beat.
- Audio-reactive treatment: subtle — the FORGE chip glow and the end-card wordmark presence breathe with the music RMS. No waveform, no equalizer bars, no pulsing type.
- SFX posture: moderate, motion-matched. Key ticks while the hook line types, a soft drop per log line, a thud on the IDE reveal, a click on the compile command, a bell on the end card.
- Audio-coupled moments: hook line typing (key ticks), log lines arriving (soft drops), simulated Enter presses (clicks), end card landing (deep bell)
- Restraint rule: no SFX under the headline or the end-card text — those must read in silence.

## Storyboard

### Scene 1 — Hook: `import "llm"` — 4.02s
Forge chip (accent `#007ACC`) top-left. Headline lands at 0.05s and holds the whole scene: **"a programming language with a 135M model inside it"** (`135M` tinted `#4EC9B0`). A terminal window (VS Code Dark+ chrome) opens at 0.02s; `import "llm"` types out character by character 0.52–1.42s with key ticks; at 1.52 / 2.02 / 2.52s the three real `[LLAMA]` log lines stream in and hold together to the end of the scene.
Sequential/interaction: yes — the code line types, then three output lines appear one by one and hold as a set.
Audio intent: small, precise, "something is happening in there".
Audio-coupled idea: per-character key ticks on the typed line; a soft drop on the first log line.
Music: upbeat bed from 0.0 at 0.32.
Transition mood: clean hard cut → Scene 2 at 4.02s (`// beat-locked`).

### Scene 2 — The IDE — 4.02s
The Forge IDE, recreated: activity bar, `EXPLORER` sidebar with a file tree, tab bar with `hello.fge`, editor with syntax-highlighted Forge source (`let name = "World"`, `fn greet(person)`, `return "Hello, " + person + "!"`), a `TERMINAL`/`OUTPUT` panel printing `Hello, Forge!`, and the blue gradient status bar with `Ln 1, Col 1    Spaces: 4    Forge    UTF-8`. The IDE's real welcome tagline `write. compile. run.` fades up bottom-left at 4.4s and holds.
Sequential/interaction: yes — the IDE assembles panel by panel, then the terminal output line prints.
Audio intent: "this is a real tool", weight without noise.
Audio-coupled idea: a soft thud as the IDE frame lands; the terminal output line drops in.
Music: bed continues.
Transition mood: clean wipe → Scene 3 at 8.02s.

### Scene 3 — Compiling to bytecode — 4.0s
The terminal returns, now showing the real pipeline:
`$ forge hello.fge` → `Hello, Forge!` → `$ forgevm compile hello.fge hello.fclass` → `Compiled hello.fge -> hello.fclass` + `  135 bytes, 1 method(s)` → `$ forgevm hello.fclass` → `Hello, Forge!`.
Caption bottom-left: `source → bytecode → VM`.
Sequential/interaction: yes — each command types, each output prints, in order.
Audio intent: steady, mechanical, satisfying.
Audio-coupled idea: click on each simulated Enter; a soft drop on `Compiled ...` and on the final `Hello, Forge!`.
Music: bed continues, unchanged.
Transition mood: cut → Scene 4 at 12.02s (`// beat-locked`).

### Scene 4 — Calling native C — 4.0s
The IDE editor now shows the FFI script (`import "ffi"`, `ffi.open("./libffinative.so")`, `ffi.sym(lib, "add")`, `ffi.call_int(p_add, 2, 3)`), syntax-highlighted, while the terminal prints the real results: `add(2, 3)          = 5`, `fibonacci(20)      = 6765`, `greet              = Hello from native code, Forge!`. Caption bottom-left: `calling native C from Forge`.
Sequential/interaction: yes — a cursor clicks the IDE's `Run` button, then the three results print one by one.
Audio intent: "it reaches outside the VM and comes back".
Audio-coupled idea: mouse click on `Run`; soft drop per result line.
Music: bed continues.
Transition mood: cut → Scene 5 at 16.02s (`// beat-locked`).

### Scene 5 — The agent, then the wordmark — 4.0s
Terminal: `llm.faw_tool("calculate", "expression", "2 + 3 * 7 - 4 / 2")` types out, then `21` prints in accent. At 18.02s the end card takes over: the FORGE wordmark in `#007ACC`, `Compiles to bytecode. Runs on a stack-based VM.`, and `github.com/Nohat1337/FORGE`. Music fades under the final beat.
Sequential/interaction: yes — the call types, the result prints, the card assembles.
Audio intent: the payoff, then quiet.
Audio-coupled idea: click on the call; a deep bell as the wordmark locks up at 18.02s.
Music: bed to 18.02, fade out by 20.02.
Transition mood: hard cut → end card, then hold to 20.02.

**Music mood for this video:** upbeat but restrained — a dev-tool bed, not a hype trailer.
**Audio summary:** one 120 BPM bed running under a terminal-driven story, with key ticks and soft drops marking each action and a single deep bell landing the wordmark.
