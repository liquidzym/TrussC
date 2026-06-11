# FLUX.2-klein Starter

This folder contains the first FLUX.2-klein JSON job entry for script, Node-adjacent, and future C++ examples.

Download the model assets:

```powershell
python ..\..\tools\setup_sd.py download-model --model flux2-klein-4b-q4_0
```

Run the starter job:

```powershell
python ..\..\tools\tcxsd_job.py run jobs\flux2_klein_product_job.json
```

The job uses `persistent_server` so the same runtime path can be mirrored by pure C++ apps through `tcx::sd::ExecutionMode::PersistentServer`.
