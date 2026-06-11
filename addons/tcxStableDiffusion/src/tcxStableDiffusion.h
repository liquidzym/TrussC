#pragma once

#include "tcxsd/Generator.h"
#include "tcxsd/NativeRuntime.h"
#include "tcxsd/Types.h"

namespace tcx {

using StableDiffusion = sd::StableDiffusion;
using StableDiffusionImage = sd::ImageResult;
using StableDiffusionRequest = sd::ImageRequest;
using StableDiffusionModelPaths = sd::ModelPaths;
using StableDiffusionRuntimeSettings = sd::RuntimeSettings;
using StableDiffusionModelProfile = sd::ModelProfile;
using StableDiffusionStorageRoots = sd::StorageRoots;
using StableDiffusionCleanupOptions = sd::CleanupOptions;
using StableDiffusionCleanupResult = sd::CleanupResult;
using StableDiffusionBackendCapabilities = sd::BackendCapabilities;
using StableDiffusionCanvasDefaults = sd::CanvasDefaults;
using StableDiffusionPromptPack = sd::PromptPack;
using StableDiffusionGenerationProject = sd::GenerationProject;
using StableDiffusionGenerationArtifact = sd::GenerationArtifact;
using StableDiffusionBatchJob = sd::BatchJob;
using StableDiffusionVariantJob = sd::VariantJob;
using StableDiffusionGenerationSession = sd::GenerationSession;
using IdeogramPrompt = sd::IdeogramPrompt;

} // namespace tcx
