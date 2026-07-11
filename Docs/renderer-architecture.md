# Graphics renderer architecture decision

Status: experimental implementation, 2026-07-10

## Problem

OpenCPN historically selects between a wxWidgets software chart canvas and a
large OpenGL chart canvas using one Boolean setting. On multi-GPU Linux systems
that leaves adapter selection to GL vendor dispatch. A failed PRIME, GLX, Zink,
or proprietary-driver path can produce a black canvas or, in the worst case, a
system-level GPU lock. Navigation state must remain usable when acceleration is
unavailable.

## Decision

OpenCPN now has an explicit renderer configuration with three mutually
exclusive backends:

1. **Software renderer**: the existing complete wxWidgets chart renderer.
2. **OpenGL renderer (legacy)**: the existing OpenGL implementation, unchanged.
3. **Vulkan presentation (experimental)**: the complete software chart renderer
   produces the authoritative frame; a small Vulkan presenter transfers that
   frame to a swapchain without using GL, GLX, Zink, shaders, or cross-device
   sharing.

The new backend is intentionally a presentation accelerator, not a second
nautical chart renderer. Raster/vector charts, S52 symbols, text, routes,
tracks, waypoints, AIS, overlays, chart quilting, rotation, colour schemes and
high-DPI layout continue through the mature software path. This preserves
feature parity and keeps chart-derived geometry away from a new GPU pipeline.

The Linux Vulkan loader is restricted before instance creation to the selected
driver family. The default policy accepts only a present-capable Intel
integrated adapter (`vendorID == 0x8086` and Vulkan integrated-device type).
NVIDIA (`vendorID == 0x10de`) is only considered after an explicit user
selection. Failure never changes the policy to another GPU.

The presenter uses FIFO presentation, one frame in flight, a bounded
host-visible staging buffer, transfer-destination swapchain images, bounded
waits, and no shader or geometry pipeline. A frame exceeding the configured
upload limit is rejected before allocation.

## Alternatives considered

### Port the OpenGL chart canvas directly to Vulkan

Rejected for this milestone. `glChartCanvas` owns chart textures, S52 drawing,
labels, symbols, tessellation, clipping, overlays and plugin GL callbacks. A
direct port would duplicate a navigation-critical renderer and could not reach
feature parity in a reviewable change.

### SDL3 GPU

Not selected. SDL GPU is maintained and cross-platform, but it adds a second
window-system abstraction beside wxWidgets and its public device creation API
does not provide the strict vendor/device selection policy required here. Its
2D renderer may also choose OpenGL, which is prohibited for this backend.

### wgpu

Not selected. It provides mature validation and multiple backends but adds a
large Rust/C dependency boundary and still requires native-window integration.
It does not remove the need to preserve or rewrite OpenCPN's chart renderer.

### Skia

Not selected. Skia could replace much of the 2D software renderer, but doing so
would be a broad rendering rewrite with a large dependency and packaging cost.
It does not by itself solve deterministic Vulkan adapter selection.

### Prepared out-of-process renderer

Deferred. Process isolation would protect OpenCPN from a userspace renderer
crash, but a GPU driver can still lock the kernel. Embedding a foreign renderer
surface is also not portable on Wayland, and copying every full chart frame over
IPC adds substantial complexity. The current presenter owns no navigation
state and has a narrow frame boundary, making later shared-memory process
isolation possible without moving GPS, AIS, routes, waypoints, alarms or chart
selection out of the main application.

## Failure and fallback model

- Device/adapter/surface/swapchain/staging failures fall back to software.
- Device loss, out-of-memory and bounded-wait timeout fall back immediately;
  there is no device-recreation loop.
- After device loss or a fence timeout, the failed Vulkan device is abandoned
  rather than entering a potentially blocking device-idle teardown. Its driver
  resources are reclaimed when the process exits; the running navigation core
  continues with software presentation.
- A surface resize may recreate the swapchain once as part of normal sizing.
- A startup marker is removed only after the first successful presentation.
  If it remains after an unclean shutdown, the next normal launch uses software.
- `--renderer=vulkan-experimental` is an explicit retry and may bypass that
  marker. `--disable-experimental-renderer` disables it for one launch.
- `--renderer=software` is the safe-start override. `--reset-renderer` clears
  renderer settings and the startup marker without resetting other settings.
- There is no automatic Intel-to-NVIDIA fallback.

Because a kernel GPU lock is outside application control, software remains the
recommended renderer until Intel hardware testing has completed.

## Configuration and migration

The configuration records backend, adapter policy, fallback, diagnostics,
restart policy and bounded resource limits. If `RendererBackend` is absent, the
old `OpenGL` Boolean migrates to either `opengl-legacy` or `software`. The old
key is still written for compatibility.

Stable values:

- backends: `software`, `opengl-legacy`, `vulkan-experimental`
- adapters: `intel-integrated`, `cpu-software`, `nvidia-experimental`

## Security and data validation

Dimensions and multiplication are validated before allocation. Uploads,
staging memory, frames in flight and wait times are bounded. No chart-generated
GPU geometry, shader source, CUDA object, or foreign texture handle enters the
experimental backend.

## Testing strategy

- deterministic adapter inventories verify Intel selection and prove NVIDIA is
  never selected implicitly;
- configuration migration, parsing, fallback order and resource limits are
  unit tested;
- device/surface/process failure decisions are tested without loading an ICD;
- software and legacy OpenGL builds remain available;
- Lavapipe is the intended CI/headless Vulkan target;
- Intel hardware testing is opt-in and bounded;
- NVIDIA is never used by automated tests.

## Platform implications and limitations

The first presenter implementation is wxGTK/Linux only and supports X11 and
Wayland Vulkan surfaces. Other platforms compile with software and legacy
OpenGL choices; the Vulkan choice safely falls back when not built. Vulkan
presentation currently uploads a complete software frame, so it does not
accelerate S52/chart construction and can be slower than software blitting on
some systems. Plugin OpenGL overlays remain part of the legacy OpenGL backend
and are not invoked by Vulkan presentation.

Required Linux build packages are Vulkan headers/loader and GTK3 development
files. Safe testing additionally requires an Intel ICD (`vulkan-intel`) or the
Lavapipe software ICD (`vulkan-swrast`), plus validation layers for diagnostic
builds.

## Future work

- shared-memory out-of-process presentation where the window system supports
  reliable embedding;
- persistent renderer health panel and bounded frame-time history;
- partial-damage uploads and a reusable texture cache;
- physical Intel Wayland/X11 qualification and suspend/resume testing;
- only after that, selective GPU-native primitives which retain software
  fallback and golden-image parity.

## References

- [Vulkan physical devices and queue families](https://docs.vulkan.org/spec/latest/chapters/devsandqueues.html)
- [Khronos Vulkan Loader driver and layer filtering](https://github.com/KhronosGroup/Vulkan-Loader/blob/main/docs/LoaderInterfaceArchitecture.md)
- [Vulkan swapchain presentation](https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/01_Presentation/01_Swap_chain.html)
- [SDL3 GPU API](https://wiki.libsdl.org/SDL3/CategoryGPU)
- [OpenCPN issue 1549: Vulkan support](https://github.com/OpenCPN/OpenCPN/issues/1549)
