# Z-Image Turbo Starter

This folder contains the first Z-Image Turbo JSON job entry for script and Node workflows. The full GUI selector lives in `..\ideogram4-basic`.

Download the model assets:

```powershell
python ..\..\tools\setup_sd.py download-model --model z-image-turbo-q3_k
```

The downloader places files in `..\ideogram4-basic\bin\data\models\z-image-turbo-q3_k`.

Run the starter job:

```powershell
python ..\..\tools\tcxsd_job.py run jobs\z_image_turbo_wide_job.json
```

The job uses `persistent_server`; pure C++ apps can use the same model profile through `tcx::StableDiffusion::setupZImageTurbo()`.
