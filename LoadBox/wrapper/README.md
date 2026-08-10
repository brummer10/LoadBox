
# Plugin Wrapper (VST3 / CLAP / VST2)

Format-agnostic VST3, CLAP and VST2 host wrappers built around a single
C++ interface (`PluginAPI.h`).

## Licensing of the vendored headers

Each format's vendored SDK header carries its own license, independent
of the BSD-3-Clause license `PluginAPI.h`, `Parameter.h` and the three `*Wrapper.cpp` files comes with.

| Folder | Upstream project | License | Notes |
|---|---|---|---|
| `vst3/travesty/` | [DISTRHO/DPF's Travesty](https://github.com/DISTRHO/DPF/tree/main/distrho/src/travesty) | **ISC** (permissive, GPL-compatible) | Clean-room VST3 C API, avoids the Steinberg VST3 SDK's own (GPLv3-or-commercial) license entirely. `LICENSE` file included in the folder. |
| `clap/clap/` | [free-audio/clap](https://github.com/free-audio/clap) | **MIT** (permissive) | Official upstream CLAP headers. |
| `vst2/vestige.h` | Javier Serrano Polo's VeStige header | **GPLv2-or-later** | Copyleft, unlike the other two. If you link this into your plugin, the resulting binary is a GPL derivative work - keep that in mind for closed-source VST2 builds. The header's own comment block also carries this explicit disclaimer: it was written without reference to Steinberg's VST2 SDK or agreement to its license. |

Practically: 
VST3 and CLAP builds here carry no copyleft obligation from these headers, 
VST2 does, because of the GPLv2-or-later license of `vestige.h`.
