# iOS / iPadOS Touch Integration Notes

## Entry Point

```cpp
#include "tcxMapWrap/tcxMapWrap.h"
using namespace tcx::mapwrap;

// In your iOS app delegate or tcApp:
MapWrapEngine mapper;

void setup() {
    MapWrapI18n::instance().detectAndSetLanguage();
    // ...
}
```

## Touch Event → PointerEvent

```objc
// In your UIViewController or GLKViewController:
- (void)touchesBegan:(NSSet*)touches withEvent:(UIEvent*)event {
    for (UITouch* touch in touches) {
        CGPoint pos = [touch locationInView:self.view];
        PointerEvent e = PointerEvent::touch(
            Vec2(pos.x, pos.y),
            (int)(intptr_t)touch
        );
        e.type = PointerEvent::Type::Down;
        mapper.editor().pointerDown(e);
    }
}

- (void)touchesMoved:(NSSet*)touches withEvent:(UIEvent*)event {
    for (UITouch* touch in touches) {
        CGPoint pos = [touch locationInView:self.view];
        PointerEvent e = PointerEvent::touch(
            Vec2(pos.x, pos.y),
            (int)(intptr_t)touch
        );
        e.type = PointerEvent::Type::Move;
        mapper.editor().pointerMove(e);
    }
}

- (void)touchesEnded:(NSSet*)touches withEvent:(UIEvent*)event {
    for (UITouch* touch in touches) {
        CGPoint pos = [touch locationInView:self.view];
        PointerEvent e = PointerEvent::touch(
            Vec2(pos.x, pos.y),
            (int)(intptr_t)touch
        );
        e.type = PointerEvent::Type::Up;
        mapper.editor().pointerUp(e);
    }
}
```

## Editor Viewport Pinch Zoom / Pan

Two-finger gestures are handled by tracking multiple PointerEvents:

- When 2 active pointers exist, compute pan delta and zoom scale
- Call `editor.viewport().panBy(delta)` and `editor.viewport().zoomAt(center, scale)`

## Touch Handle Radius

- Default touch hit radius: **24px** (vs 8px for mouse)
- Configurable in `OverlayOptions::touchHandleRadiusPixels`
- Radius is calculated in **screen pixels**, not canvas normalized coords
- After zoom, touch radius remains in screen pixel space

## File Save Strategy

```cpp
// iOS: save to app documents directory
std::string docsDir = getExecutableDir() + "Documents/";
MapWrapSerialization::saveToFile(mapper.document(), docsDir + "project.json");
```

Do NOT use system file picker from the addon. The host app manages file paths.

## Autosave Path Strategy

```cpp
AutosaveSettings autosave;
autosave.enabled = true;
autosave.intervalSeconds = 30.0f;
autosave.autosaveFolder = docsDir + "autosave/";
mapperAutosave.setup(&mapper.document(), autosave);
```

## Language Detection

On iOS, `MapWrapI18n::detectAndSetLanguage()` uses `[[NSLocale preferredLanguages] firstObject]` to determine:
- zh-Hans, zh-Hant, zh_CN, zh_TW, zh-HK → Chinese
- Everything else → English

Manual override:
```objc
// Swift bridge:
MapWrapI18n::instance().setLanguage("zh");
MapWrapI18n::instance().setLanguage("en");
```

## Future iPad UI Suggestions

- Use Swift/ObjC++ bridge for UI layer
- Core API is pure C++ with minimal templates
- No platform UI objects stored in core data structures
- All editing accessible via `MapWrapEditor` methods
- No keyboard dependency for core operations
