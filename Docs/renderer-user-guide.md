# Graphics renderer recovery and diagnostics

Renderer selection is under **Settings → Display → Advanced → Graphics
Renderer**.

- **Software renderer** is the safest option and supports all chart features.
- **OpenGL renderer (legacy)** preserves the previous accelerated canvas.
- **Vulkan presentation (experimental)** uses the software chart renderer and
  presents its finished frames through a specifically selected Vulkan device.

The default Vulkan adapter policy is **Intel integrated GPU (recommended)**.
The NVIDIA option is experimental and opt-in. OpenCPN never changes from Intel
to NVIDIA as a fallback.

The settings page reports the active backend, device, driver and health state.
Renderer changes take effect after restarting OpenCPN. This avoids leaving a
chart canvas half-attached to two different window-system rendering backends.

## Safe start

```sh
opencpn --renderer=software
opencpn --disable-experimental-renderer
opencpn --reset-renderer
```

To make one explicit experimental retry after a failed startup:

```sh
opencpn --renderer=vulkan-experimental
```

If OpenCPN did not complete the first Vulkan presentation on the previous run,
the next ordinary launch starts in software mode and logs the reason. It does
not enter a renderer restart loop.

## Diagnostics

Startup logs include the requested backend, adapter policy, selected vendor and
device IDs, device type, driver/API version, FIFO presentation mode, resource
limits, validation status and any fallback decision. Search the OpenCPN log:

```sh
rg "Renderer:" ~/.opencpn/opencpn.log
```

On Arch Linux, safe Vulkan testing requires `vulkan-intel` for Intel hardware or
`vulkan-swrast` for CPU-only Lavapipe testing. Do not use `DRI_PRIME=1`,
`prime-run`, Zink or an NVIDIA ICD merely to test the Intel policy.

The experimental presenter does not maintain a GPU texture cache: its bounded
allocation is a single full-frame staging upload. The legacy texture-cache
setting continues to apply only to the legacy OpenGL renderer.
