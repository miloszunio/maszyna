fix(gl46): the loading screen stays until the shaders are compiled, no black window at the first start of a new version

## What was wrong

At the first start of a new build, the window went black and stopped answering for about half a minute after the scenery was loaded. Windows marked it "not responding".

The driver keeps the compiled shaders of each executable apart, and every build has a name of its own (`eu07-x64_<date>_<hash>.exe`). So every first start of a build compiles everything anew. The second start of the same executable is quick.

Measured on `calkowo_osobowy` with a copy of the executable under a new name (nothing kept by the driver), with the frame timed part by part:

- 33 of those seconds were the first step of the water on the windscreens. It compiled its nine programs (`water_*.comp`) in the first frame of the simulation, in `opengl46_windscreen_water::gpu_init()`.
- The rest of the first frame took about 1.5 s.

## What changes

- After the scenery is loaded, the loader keeps its screen, with the bar going and the note "Preparing the graphics (takes longer at the first start of a new version)", until the renderer says its programs are ready. This is the new `gfx_renderer::Prewarm_Shaders()`, called for about 50 ms a frame.
- The programs of the materials are drawn with once each into a small 4×4 target of the formats and samples of the colour pass. Each is drawn as the opaque things are and as the translucent ones are, with a triangle of no area.
- Then the programs of the windscreen water are compiled one a frame (`opengl46_windscreen_water::prepare_next()`). `gpu_init()` keeps the ones made ahead. The window answers in between.
- The Vulkan renderer, which has its pipeline cache, uses the default of the interface: it's all done at once.
- The note is translated in `lang/editor_pl.po`.

## Checked

- With a copy of the executable under a new name: the loading screen with the note stays from the end of the loading (19 s) to the first frame (58 s), and the window answers all the time. The first frame no longer stalls: the water takes 12 ms instead of 33 s.
- With the shaders kept by the driver, the preparing takes no time.
- Built with MSVC on this branch with `WITH_WASM=OFF` (the `ref/wamr` submodule couldn't be fetched here). The change doesn't touch the WebAssembly screens. CI builds the whole thing with MSVC and GCC.
- Developed and checked on `ficzery2` (32ca2411). There, the loader already had the closing frames with the note (e0a52e2a), which this branch doesn't have, so they come with this PR.

No asset changes. Nothing changes in any file format.

🤖 Generated with [Claude Code](https://claude.com/claude-code)
