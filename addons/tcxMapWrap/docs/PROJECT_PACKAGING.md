# Project Packaging

## Relative Path Strategy

All media paths in the JSON are stored as **relative to the composition file**:

```json
{
  "id": "src_image_1",
  "kind": "image",
  "path": "media/test.png"
}
```

When loading, relative paths are resolved against the composition file's directory.

## Validate Project

```cpp
auto report = mapper.document().validateProject();
// report.missingSources — sources referenced but not found
// report.missingFiles — media files not found on disk
// report.warnings — other issues
```

## Relink Source

```cpp
auto result = ProjectPackaging::relinkSource(mapper.document(), "src_image_1", "/new/path/image.png");
```

## Collect Media

```cpp
// Packages all media into a folder alongside the composition:
auto result = ProjectPackaging::collectMediaToFolder(mapper.document(), "packaged_project/");
// Creates:
//   packaged_project/project.tcxmap.json
//   packaged_project/media/test.png
```

## iOS Sandbox Notes

- Do NOT access paths outside the app sandbox
- The host app provides all file paths to the addon
- Do NOT use system file picker from within the addon
- `validateProject()` checks if files exist at the provided paths
- It does NOT attempt to access paths outside the sandbox
