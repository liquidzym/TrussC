# Runtime Workflow Encapsulation Roadmap

This document is the execution checklist for the production workflow layer.

Hard constraints:

- Python is test and development tooling only. Released programs, Node users, and
  C++ examples must not depend on Python at runtime.
- Each feature must have a real C++ and/or Node interface, real example usage,
  real sidecar/artifact metadata, and structured unsupported-capability errors
  when a backend cannot execute it.
- This is not an MVP pass. Do not add decorative placeholders that look like
  product features but cannot run.
- ControlNet must use a real local model asset under `bin/data/models`, and the
  example smoke must generate through the ControlNet path.

## Layer 1: Generation Capabilities

- Add typed request factories for:
  - `TextToImage`
  - `ImageToImage`
  - `Inpaint`
  - `ControlNet`
  - `LoRAStack`
  - `Refine` / `Upscale`
- Preserve the low-level `ImageRequest` escape hatch.
- Add capability checks so each backend reports support before execution.
- Write sidecar metadata for mode, source image, mask image, control image,
  LoRA stack, refine source, strength, control strength, and backend capability.
- Surface the same request modes in the Node package without Python.

## Layer 2: Workflow Objects

- Add `GenerationProject` for explicit output/temp/cache/log roots.
- Add `Artifact` for image path, sidecar path, metadata, parent lineage,
  quality report, and source inputs.
- Add `GenerationSession` as the reusable runtime object around model,
  runtime settings, storage roots, and server reuse.
- Add `BatchJob` for prompt/model/quality sweeps.
- Add `VariantJob` for source-artifact-based variations.
- Show these objects in example code and the workbench UI.

## Layer 3: User Intent Presets

- Add prompt packs for poster, product shot, wide scene, game asset, and UI
  mockup style tasks.
- Add style presets, canvas presets, text rendering presets, and a model router.
- Add quality gates that can reject or warn on placeholder prompts, missing
  outputs, wrong size, blank/tiny images, and text-rendering failures.
- Keep Chinese prompt and visible-text flows as first-class examples.

## Example Program Requirements

- Update `examples/ideogram4-basic` into the main workflow workbench.
- Include task modes for text-to-image, image-to-image, inpaint, ControlNet,
  LoRA, refine/upscale, batch, and variant workflows.
- Update the ImGui theme to a dark charcoal, warm gray, and earthy-gold palette.
- Keep Chinese labels readable through the existing CJK font loading path.
- Add smoke-mode switches for every executable workflow that can be run in CI or
  local scripted verification.
- Include smoke-mode preprocessing for ControlNet guide generation and inpaint
  mask generation so image-input workflows can be verified without loading a
  model.
- Include LoRA directory scanning and path normalization in the UI and Node
  package so LoRA requests submit server-relative asset names.

## Model Asset Layout

Runtime assets live under the example data root:

```text
examples/ideogram4-basic/bin/data/models/
  ideogram4-q4_0/
  flux2-klein-4b-q4_0/
  z-image-turbo-q3_k/
  sd15-controlnet-canny/
    v1-5-pruned-emaonly.safetensors
    control_v11p_sd15_canny_fp16.safetensors
```

If a required asset is absent, runtime APIs must return
`MODEL_ASSET_MISSING` with remediation hints. Test tooling may use Python to
download or verify assets, but release examples must only consume the files.

## Review And Verification

Before committing:

1. Run unit and syntax checks for C++-adjacent tests, Node, and Python test
   utilities.
2. Build the TrussC example with `trusscli build`.
3. Run real smoke generation for text-to-image and ControlNet.
4. Review the full diff once for API correctness.
5. Review the full diff a second time for runtime behavior, paths, assets,
   UI, and docs.
6. Fix findings from both reviews before commit.
