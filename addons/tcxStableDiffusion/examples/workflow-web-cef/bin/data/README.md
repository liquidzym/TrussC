# workflow-web-cef data

This folder is the direct-launch asset root for the packaged example.

- `models/`: model profiles and LoRA files used by the bundled Node worker.
- `inputs/`: deterministic local input images for ControlNet, img2img, and inpaint.
- `workflows/outputs/`: generated PNG files and sidecar JSON files.
- `workflows/tmp/`: temporary request and backend scratch files.
- `workflows/cache/`: reusable local cache files.
- `workflows/logs/`: native `sd-server` logs and worker diagnostics.

The runtime never calls developer setup scripts. Developer tools may stage assets
here before packaging, and the Node package cleanup API owns lifecycle cleanup
for outputs, temp files, cache files, sidecars, and logs.
