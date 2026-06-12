# ios-ar-vision-coreml

TrussC iOS example for `tcxIOS` ARKit, Vision, and CoreML bridges.

Keyboard shortcuts:

- `A`: start AR world tracking.
- `V`: run Vision rectangle detection on `/tmp/tcxios-vision.png`.
- `S`: generate a foreground-instance subject mask for `/tmp/tcxios-vision.png`.
- `M`: inspect `/tmp/tcxios-model.mlmodelc`.

ARKit requires an AR-capable real device for meaningful validation. Vision and
CoreML require real image/model paths. Foreground-instance masks require iOS 17
or newer; apps can use `VisionMaskKind::PersonSegmentation` for the iOS 15+
person-mask path.
