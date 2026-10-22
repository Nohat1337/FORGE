# Hyperframes Composition Brief: Forge Programming Language

## Objective
Create a short launch-style brag video for **Forge**, the programming language in `/home/nohat1337/c++-fork`.

## Output
- Composition directory: `brag-output/composition/`
- Rendered video: `brag-output/brag.mp4`
- Format: landscape — 1920x1080
- Duration: 20.02s (five 4-second scenes)

## Source Material
- Project root: `/home/nohat1337/c++-fork`
- Primary files read: `README.md`, `GUIDE.md`, `src/fvm/forge_ide.cpp` (theme + IDE chrome), `src/fvm/sdl2_ui.cpp` (8×8 font + palette), `src/fvm/llm.cpp`, `src/fvm/ffi.cpp`, `src/fvm/faw.cpp`, `src/fvm/runtime.cpp` (native module registry), `src/main.cpp`, `src/forgevm_main.cpp`
- Product name: **Forge Programming Language**
- Tagline / strongest claim: `a programming language with a 135M model inside it` (derived from the real `llm` module + `models/SmolLM-135M.Q8_0.gguf`)
- Key UI or visual moment to recreate: the Forge IDE window in VS Code Dark+ (activity bar, EXPLORER, tabs, editor, terminal panel, blue gradient status bar) and its terminal
- Copy that must appear verbatim:
  - `import "llm"`
  - `[LLAMA] Loading: models/SmolLM-135M.Q8_0.gguf`
  - `[LLAMA] Vocab=49152 Context=2048`
  - `[LLAMA] Ready!`
  - `Hello, Forge!`
  - `$ forge hello.fge`
  - `$ forgevm compile hello.fge hello.fclass`
  - `Compiled hello.fge -> hello.fclass`
  - `  135 bytes, 1 method(s)`
  - `$ forgevm hello.fclass`
  - `add(2, 3)          = 5`
  - `fibonacci(20)      = 6765`
  - `greet              = Hello from native code, Forge!`
  - `llm.faw_tool("calculate", "expression", "2 + 3 * 7 - 4 / 2")`
  - `21`
  - `write. compile. run.`
  - `Ln 1, Col 1    Spaces: 4    Forge    UTF-8`
  - `EXPLORER` · `TERMINAL` · `OUTPUT` · `Build` · `Run` · `Save` · `New`
  - `Compiles to bytecode. Runs on a stack-based VM.`
  - `github.com/Nohat1337/FORGE`

All terminal output above is verbatim captured output from the built binaries in this repository (see the verification runs recorded in `brag-output/brag-plan.md`).

## Creative Direction
- Tone preset: `default`
- Creative direction: confident developer-tool launch — the terminal is the hero, the IDE is the proof, no hype adjectives, real output on screen
- Interpretation: playful but factual. Fast entrances, slow holds, every string legible. The camera never leaves the product's own UI, so it reads as a screencast of something real rather than a marketing render.
- Angle: a programming language whose runtime loads a 135M-parameter neural network. The hook is the surprise (`import "llm"` → real model-loader logs); the middle proves the toolchain (IDE, compiler, native C interop); the end card lands the wordmark.
- Hook: frame 0 carries the headline `a programming language with a 135M model inside it`; the terminal types `import "llm"` and prints the real `[LLAMA]` loader logs.
- Outro / punchline: the agent's tool call resolves to `21`, then the FORGE wordmark + `Compiles to bytecode. Runs on a stack-based VM.` + `github.com/Nohat1337/FORGE`.
- Avoid:
  - Generic SaaS language ("streamline your workflow", "developer productivity", "supercharge")
  - Abstract filler visuals (color washes, particles, waveform bars)
  - Unrelated visual redesign — everything must look like the real Forge IDE/terminal
  - Any claim the repo does not make (no "10x faster", no GPU matmul, no JIT claims)

## Visual Identity
- Background: `#1E1E1E`
- Panel: `#252526` · Activity bar `#333333` · Title bar `#373738`
- Accent: `#007ACC` (status bar gradient `#007ACC` → `#005690`)
- Text: `#D4D4D4` · dim `#858585` · bright `#E7E7E7`
- Syntax colors (verbatim VS Code Dark+): keyword `#569CD6`, control `#C586C0`, type `#4EC9B0`, function `#DCDCAA`, string `#CE9178`, number `#B5CEA8`, comment `#6A9955`
- Display font: local DejaVu Sans Bold via `@font-face` (`assets/fonts/DejaVuSans-Bold.ttf`), tracked-out uppercase for the FORGE lockup — the real IDE wordmark is an 8×8 bitmap face at 6× scale, so a spaced uppercase lockup is the faithful translation
- Body/terminal font: local JetBrains Mono via `@font-face` (`assets/fonts/JetBrainsMonoNLNerdFontMono-Regular.ttf`)
- Visual references from the project: `src/fvm/forge_ide.cpp` theme tables, IDE layout diagram in `forge_ide.hpp`, the `FORGE` welcome wordmark, the `forge`/`forgevm` CLI output, the SDL2 splash logo box color `#62A0EA`

## Storyboard
Use the storyboard in `brag-output/brag-plan.md` as the creative contract.

Scene summary:
1. Hook — `import "llm"` — 4.02s — headline + Forge chip + terminal typing `import "llm"` + three real `[LLAMA]` log lines
2. The IDE — 4.02s — recreated Forge IDE (activity bar, EXPLORER, tabs, highlighted source, TERMINAL panel with `Hello, Forge!`, gradient status bar) + tagline `write. compile. run.`
3. Compiling to bytecode — 4.0s — `$ forge hello.fge` → `Hello, Forge!` → `forgevm compile` → `Compiled …` + `135 bytes, 1 method(s)` → `forgevm hello.fclass` → `Hello, Forge!`
4. Calling native C — 4.0s — FFI source in the editor, cursor clicks `Run`, results print: `add(2, 3) = 5`, `fibonacci(20) = 6765`, `greet = Hello from native code, Forge!`
5. The agent + end card — 4.0s — `llm.faw_tool("calculate", …)` → `21`, then FORGE wordmark + tagline + `github.com/Nohat1337/FORGE`

## Audio
- Audio role: warm bed + motion-matched UI accents
- Audio arc: bed from 0.0, key ticks and drops under each terminal action, one deep bell on the wordmark at 18.02s, bed fades out by 20.02s
- Music: `happy-beats-business-moves-vol-1-by-ende-dot-app.mp3`
- Music treatment: volume 0.32, fade to 0 across the final 1.2s so the end card lands in near-silence under the bell
- Music cue guidance: bundled preset at `brag-output/composition/assets/music/cues/happy-beats-business-moves-vol-1-by-ende-dot-app.music-cues.json` (120.19 BPM). Lock scene boundaries to 4.02 / 8.02 / 12.02 / 16.02 / 20.02 (16.02 and 20.02 are strong cues). Small entrances snap to the half-beat grid (x.02 / x.52). Sequential terminal text is revealed quickly and held as a set — never one line per beat.
- Audio-reactive treatment: subtle — the FORGE chip glow and the end-card wordmark presence breathe with the extracted RMS. No waveform, equalizer, or pulsing type.
- Audio-coupled moments:
  - Scene 1 — the `import "llm"` line types → per-character key ticks; first log line → soft drop
  - Scene 3 — each simulated Enter → click; `Compiled …` and the final `Hello, Forge!` → soft drop
  - Scene 4 — cursor click on `Run` → mouse click; each result line → soft drop
  - Scene 5 — the tool call types → key ticks; `21` prints → soft drop; wordmark locks up at 18.02s → deep bell
- SFX selection guidance: keyboard ticks are sparse and only on the typed characters; drops are low-frequency and short; the end-card bell is the only large sound in the video.
- SFX analysis guidance: `/home/nohat1337/.config/opencode/skills/brag-main/skills/brag/assets/sfx/sfx-analysis.md` — prefer low high-frequency-risk files for the repeated typing/drop moments.
- Exact SFX choice: Hyperframes should choose filenames, timestamps, density, and volume based on the implemented animation.
- Audio files: copy the chosen music and the Hyperframes-selected SFX into `brag-output/composition/assets/`

## Hyperframes Instructions
Load the composition-building Hyperframes domain skills — `hyperframes-core` (composition contract + `data-*` timing), `hyperframes-animation` (motion), `hyperframes-creative` (design spec, beats, audio-reactive), `hyperframes-keyframes` (seek-safe keyframes), and `hyperframes-cli` (lint/check/render). /brag is its own workflow: do not enter the `hyperframes` entry-point intent interview and do not route into its generic promo / launch-video workflow. Prefer native Hyperframes conventions over anything in `/brag`.

Requirements:
- Show at least one real UI, copy, or visual element from the source project — the terminal output and IDE chrome are both real product surfaces.
- Keep all text readable in the final render.
- Keep the video within 15-25 seconds.
- Include the planned music/SFX layer.
- Treat `/brag` audio notes as guidance, not a fixed cue sheet. Choose SFX after the visual animation exists.
- Treat music cue metadata as optional timing hints.
- Major reveals may move toward nearby strong cues within about 0.15s. Smaller entrances may align to nearby beat points within about 0.10s. Use only 1-3 strong cue locks in a 15-25s video unless the edit clearly benefits from more.
- Use SFX to support motion and interaction.
- Honor planned music treatment such as fade-outs and letting the final SFX ring over the music.
- When music is present and the treatment is not `none`, use the `hyperframes-creative` audio-reactive workflow: extract audio data with that skill's own helper and wire at least one visual element to it.
- Use local assets for audio, fonts, and any runtime/media dependencies.
- Run `hyperframes check` before render — it is brag's single gate.
- Keep creation and rendering local. Remote or publishing workflows require a separate explicit user request.
