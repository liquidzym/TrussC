# Direct In-Process Debug Notes

## Current Finding

Direct Windows CUDA generation in the TrussC/D3D app process previously reached:

```text
decode_first_stage completed
```

and then did not return to the addon. In upstream `stable-diffusion.cpp`, that log happens after VAE decode but before the function allocates and returns the final `sd_image_t*` array.

Relevant upstream flow:

```text
stable-diffusion.cpp
  generate_image(...)
    generate_image_internal(...)
      decode_images(...)
        decode_first_stage completed
        calloc result sd_image_t[]
        tensor_to_sd_image(...)
        return result_images
    generate_image completed
```

The same model files complete through:

- upstream `sd-cli.exe`,
- managed persistent `sd-server.exe`,
- the Node-facing package that talks to `sd-server.exe`.

That narrows the issue to the direct API path inside the TrussC/D3D process, not to model assets, CUDA availability, prompt parsing, or VAE decode itself.

## Current Protection

Windows CUDA `Auto` prefers the managed `sd-server.exe` backend. Normal TrussC/C++ app usage does not enter direct in-process generation.

`ExecutionMode::InProcess` remains available for controlled diagnostics only.

The direct path now emits progress breadcrumbs around the blocking call:

```text
direct in-process generate_image enter
direct in-process generate_image returned
```

If the first line appears without the second, the hang is still inside upstream `generate_image()`.

## Working Hypotheses

1. GPU runtime interaction between TrussC's D3D app process and stable-diffusion.cpp CUDA/ggml cleanup or tensor conversion after VAE decode.
2. Upstream global callbacks or logging state interacting badly with the host process after decode.
3. Memory ownership or allocation behavior around `tensor_to_sd_image(...)` only surfacing in the embedded app process.

These are hypotheses, not confirmed root causes.

## Safe Next Diagnostic

Do not run direct diagnostics in the main GUI without a watchdog. Use a small isolated command or example process with:

- `ExecutionMode::InProcess`,
- a short 512x512 / low-step job,
- progress log capture,
- an external timeout that kills the process if `generate_image returned` never appears.

Only after that reproducer is stable should we add more invasive instrumentation around upstream `tensor_to_sd_image(...)` or callback teardown.
