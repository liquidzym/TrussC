// =============================================================================
// tcxMapWrap Internationalization (i18n) — Implementation
// =============================================================================

#include "tcxMapWrap/MapWrapI18n.h"

#include <algorithm>
#include <cstdlib>

#if defined(__APPLE__)
    #include <TargetConditionals.h>
    #import <Foundation/Foundation.h>
#elif defined(_WIN32)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #include <winnls.h>
#endif

namespace tcx {
namespace mapwrap {

// ===========================================================================
// Built-in English translations
// ===========================================================================
static const MapWrapI18n::TranslationMap sEnglishTranslations = {
    // --- Edit modes ---
    {"mode.presentation",       "Presentation"},
    {"mode.surface_edit",       "Surface Editing"},
    {"mode.texture_edit",       "Texture Editing"},
    {"mode.source_assign",      "Source Assignment"},
    {"mode.mask_edit",          "Mask Editing"},
    {"mode.output_edit",        "Output Editing"},

    // --- Surface types ---
    {"surface.quad",            "Quad"},
    {"surface.grid",            "Grid"},
    {"surface.bezier",          "Bezier Surface"},
    {"surface.triangle",        "Triangle"},
    {"surface.edit_mode",       "Edit Mode"},
    {"surface.circle",          "Circle"},
    {"surface.polygon",         "Polygon"},

    // --- Surface actions ---
    {"surface.add_quad",        "Add Quad"},
    {"surface.add_grid",        "Add Grid"},
    {"surface.add_triangle",    "Add Triangle"},
    {"surface.add_circle",      "Add Circle"},
    {"surface.add_polygon",     "Add Polygon"},
    {"surface.delete",          "Delete Surface"},
    {"surface.duplicate",       "Duplicate Surface"},
    {"surface.reset",           "Reset Surface"},
    {"surface.flip_h",          "Flip Horizontal"},
    {"surface.flip_v",          "Flip Vertical"},
    {"surface.fit_canvas",      "Fit to Canvas"},

    // --- Surface properties ---
    {"surface.name",            "Name"},
    {"surface.visible",         "Visible"},
    {"surface.locked",          "Locked"},
    {"surface.opacity",         "Opacity"},
    {"surface.source",          "Source"},
    {"surface.source_rect",     "Source Rect"},
    {"surface.layer",           "Layer"},

    // --- Grid ---
    {"grid.rows",               "Rows"},
    {"grid.cols",               "Columns"},
    {"grid.resolution",         "Resolution"},
    {"grid.add_row",            "Add Row"},
    {"grid.remove_row",         "Remove Row"},
    {"grid.add_col",            "Add Column"},
    {"grid.remove_col",         "Remove Column"},
    {"grid.interpolation",      "Interpolation"},
    {"grid.linear",             "Linear"},
    {"grid.curved",             "Curved (Catmull-Rom)"},

    // --- Circle ---
    {"circle.center",           "Center"},
    {"circle.radius_x",         "Radius X"},
    {"circle.radius_y",         "Radius Y"},
    {"circle.rotation",         "Rotation"},
    {"circle.segments",         "Segments"},

    // --- Warp ---
    {"warp.perspective",        "Perspective"},
    {"warp.grid",               "Grid (Bilinear)"},
    {"warp.bezier",             "Bezier Surface"},
    {"warp.perspective_grid",   "Perspective + Grid"},
    {"warp.none",               "None"},

    // --- Source ---
    {"source.texture",          "Texture"},
    {"source.fbo",              "FBO"},
    {"source.video",            "Video"},
    {"source.none",            "None"},
    {"source.image",            "Image"},
    {"source.generated",        "Generated"},
    {"source.builtin_pattern",  "Built-in Pattern"},
    {"source.missing",          "Missing Source"},
    {"source.clock",            "Source Clock"},
    {"source.play",             "Play"},
    {"source.pause",            "Pause"},
    {"source.stop",             "Stop"},
    {"source.loop",             "Loop"},
    {"source.seek",             "Seek"},

    // --- Calibration patterns ---
    {"pattern.checkerboard",    "Checkerboard"},
    {"pattern.grid",            "Grid"},
    {"pattern.fine_grid",       "Fine Grid"},
    {"pattern.crosshair",       "Crosshair"},
    {"pattern.corner_labels",   "Corner Labels"},
    {"pattern.uv_gradient",     "UV Gradient"},
    {"pattern.color_bars",      "Color Bars"},
    {"pattern.luma_ramp",       "Luma Ramp"},
    {"pattern.edge_blend_ramp", "Edge Blend Ramp"},
    {"pattern.alpha_radial",    "Alpha Radial"},
    {"pattern.numbered_cells",  "Numbered Cells"},
    {"pattern.safe_area",       "Safe Area"},
    {"pattern.solid_color",     "Solid Color"},

    // --- Mask ---
    {"mask.rectangle",          "Rectangle Mask"},
    {"mask.ellipse",            "Ellipse Mask"},
    {"mask.polygon",            "Polygon Mask"},
    {"mask.bezier",             "Bezier Mask"},
    {"mask.freehand",           "Freehand Mask"},
    {"mask.alpha_texture",      "Alpha Texture Mask"},
    {"mask.add",                "Add"},
    {"mask.subtract",           "Subtract"},
    {"mask.intersect",          "Intersect"},
    {"mask.inverted",           "Inverted"},
    {"mask.enabled",            "Enabled"},
    {"mask.feather",            "Feather"},
    {"mask.opacity",            "Opacity"},
    {"mask.add_mask",           "Add Mask"},
    {"mask.delete_mask",        "Delete Mask"},

    // --- Output / Stage ---
    {"output.main",             "Main Output"},
    {"output.name",             "Output Name"},
    {"output.enabled",          "Output Enabled"},
    {"output.canvas_region",    "Canvas Region"},
    {"output.display_region",   "Display Region"},
    {"output.pixel_size",       "Pixel Size"},
    {"output.content_scale",    "Content Scale"},
    {"output.rotation",         "Rotation"},
    {"output.test_pattern",     "Show Test Pattern"},
    {"output.color_correction", "Output Color Correction"},

    // --- Stage ---
    {"stage.design_canvas_size","Design Canvas Size"},
    {"stage.global_masks",      "Global Masks"},

    // --- Blend ---
    {"blend.mode",              "Blend Mode"},
    {"blend.normal",            "Normal"},
    {"blend.add",               "Add"},
    {"blend.multiply",          "Multiply"},
    {"blend.screen",            "Screen"},
    {"blend.lighten",           "Lighten"},
    {"blend.darken",            "Darken"},
    {"blend.alpha_mask",        "Alpha Mask"},
    {"blend.brightness",        "Brightness"},
    {"blend.edge",              "Edge Blend"},
    {"blend.gamma",             "Gamma"},
    {"blend.luminance",         "Luminance"},
    {"blend.exponent",          "Exponent"},

    // --- Color correction ---
    {"color.correction",        "Color Correction"},
    {"color.contrast",          "Contrast"},
    {"color.saturation",        "Saturation"},
    {"color.lift",              "Lift"},
    {"color.gain",              "Gain"},
    {"color.black_level",       "Black Level"},
    {"color.white_level",       "White Level"},
    {"color.premultiplied",     "Premultiplied Alpha"},

    // --- Editor ---
    {"editor.viewport",         "Editor Viewport"},
    {"editor.pan",              "Pan"},
    {"editor.zoom",             "Zoom"},
    {"editor.fit_view",         "Fit View"},
    {"editor.select",           "Select"},
    {"editor.deselect",         "Deselect"},
    {"editor.nudge",            "Nudge"},
    {"editor.align_left",       "Align Left"},
    {"editor.align_right",      "Align Right"},
    {"editor.align_top",        "Align Top"},
    {"editor.align_bottom",     "Align Bottom"},
    {"editor.align_center_x",   "Align Center X"},
    {"editor.align_center_y",   "Align Center Y"},
    {"editor.distribute_h",     "Distribute Horizontally"},
    {"editor.distribute_v",     "Distribute Vertically"},
    {"editor.copy_geometry",    "Copy Geometry"},
    {"editor.paste_geometry",   "Paste Geometry"},
    {"editor.copy_uv",          "Copy UV"},
    {"editor.paste_uv",         "Paste UV"},

    // --- Snap ---
    {"snap.enabled",            "Snap"},
    {"snap.canvas_edges",       "Snap to Canvas Edges"},
    {"snap.canvas_center",      "Snap to Canvas Center"},
    {"snap.other_surfaces",     "Snap to Other Surfaces"},
    {"snap.grid",               "Snap to Grid"},
    {"snap.grid_step",          "Grid Step"},
    {"snap.threshold",          "Snap Threshold"},

    // --- Overlay ---
    {"overlay.surface_outlines","Surface Outlines"},
    {"overlay.surface_names",   "Surface Names"},
    {"overlay.handles",         "Handles"},
    {"overlay.grid_points",     "Grid Points"},
    {"overlay.mask_points",     "Mask Points"},
    {"overlay.canvas_grid",     "Canvas Grid"},
    {"overlay.canvas_center",   "Canvas Center"},
    {"overlay.safe_area",       "Safe Area"},
    {"overlay.output_bounds",   "Output Bounds"},
    {"overlay.source_uv",       "Source UV"},

    // --- Group ---
    {"group.name",              "Group Name"},
    {"group.visible",           "Group Visible"},
    {"group.locked",            "Group Locked"},
    {"group.opacity",           "Group Opacity"},
    {"group.translate",         "Translate"},
    {"group.rotation",          "Rotation"},
    {"group.scale",             "Scale"},
    {"group.add",               "Add Group"},
    {"group.delete",            "Delete Group"},

    // --- Undo / Redo ---
    {"undo.undo",               "Undo"},
    {"undo.redo",               "Redo"},
    {"undo.move_point",         "Move Control Point"},
    {"undo.add_surface",        "Add Surface"},
    {"undo.delete_surface",     "Delete Surface"},
    {"undo.change_source",      "Change Source"},
    {"undo.change_blend",       "Change Blend Settings"},
    {"undo.add_mask",           "Add Mask"},
    {"undo.delete_mask",        "Delete Mask"},

    // --- Project ---
    {"project.save",            "Save"},
    {"project.load",            "Load"},
    {"project.validate",        "Validate Project"},
    {"project.validation_ok",   "Project validation passed"},
    {"project.missing_source",  "Missing source"},
    {"project.missing_media",   "Missing media file"},
    {"project.relink",          "Relink Source"},
    {"project.package",         "Package Project"},

    // --- Geometry validation ---
    {"geometry.valid",              "Geometry Valid"},
    {"geometry.self_intersecting",  "Self-intersecting geometry"},
    {"geometry.too_small",          "Geometry too small"},
    {"geometry.winding_flipped",    "Winding order flipped"},
    {"geometry.has_nan",            "Geometry contains NaN values"},
    {"geometry.repair",             "Repair Geometry"},

    // --- Stats ---
    {"stats.drawn_surfaces",    "Drawn Surfaces"},
    {"stats.skipped_surfaces",  "Skipped Surfaces"},
    {"stats.rebuilt_meshes",    "Rebuilt Meshes"},
    {"stats.missing_sources",   "Missing Sources"},
    {"stats.invalid_surfaces",  "Invalid Surfaces"},
    {"stats.mask_count",        "Masks"},

    // --- Preset / Cue ---
    {"preset.name",             "Preset Name"},
    {"cue.name",                "Cue Name"},
    {"cue.apply",               "Apply Cue"},
    {"cue.transition",          "Transition (seconds)"},

    // --- Undo commands ---
    {"command.delete_surface",       "Delete Surface"},
    {"command.create_surface",       "Create Surface"},
    {"command.move_surface",         "Move Surface"},
    {"command.move_vertex",          "Move Vertex"},
    {"command.move_uv",              "Move UV Point"},
    {"command.move_grid",            "Move Grid Point"},
    {"command.rotate",               "Rotate Surface"},
    {"command.edit",                 "Edit Surface"},
    {"command.move_handle",          "Move Handle"},
    {"command.bring_forward",        "Bring Forward"},
    {"command.send_backward",        "Send Backward"},
    {"command.nudge",                "Nudge Surface"},
    {"command.nudge_handle",         "Nudge Handle"},
    {"command.fit_to_canvas",        "Fit to Canvas"},
    {"command.align_left",           "Align Left"},
    {"command.align_right",          "Align Right"},
    {"command.align_top",            "Align Top"},
    {"command.align_bottom",         "Align Bottom"},
    {"command.align_center_x",       "Align Center X"},
    {"command.align_center_y",       "Align Center Y"},
    {"command.paste_geometry",       "Paste Geometry"},
    {"command.paste_uv",             "Paste UV"},
    {"command.edit_property",        "Edit Property"},
    {"command.set_handle_position",  "Set Handle Position"},
    {"command.convert_surface",      "Convert Surface"},
    {"command.add_column",           "Add Column"},
    {"command.remove_column",        "Remove Column"},
    {"command.add_row",              "Add Row"},
    {"command.remove_row",           "Remove Row"},
    {"command.adjust_mesh_resolution","Adjust Mesh Resolution"},

    // --- Inspector properties ---
    {"property.surface_kind",        "Surface Type"},
    {"property.name",                "Name"},
    {"property.visible",             "Visible"},
    {"property.locked",              "Locked"},
    {"property.opacity",             "Opacity"},
    {"property.blend_enabled",       "Blend Enabled"},
    {"property.blend_opacity",       "Blend Opacity"},
    {"property.blend_brightness",    "Blend Brightness"},
    {"property.cc_enabled",          "Color Correction Enabled"},
    {"property.cc_brightness",       "CC Brightness"},
    {"property.cc_contrast",         "CC Contrast"},
    {"property.cc_saturation",       "CC Saturation"},
    {"property.perspective",         "Perspective Correction"},
    {"property.dest_x_0",            "Point 0 X"},
    {"property.dest_y_0",            "Point 0 Y"},
    {"property.dest_x_1",            "Point 1 X"},
    {"property.dest_y_1",            "Point 1 Y"},
    {"property.dest_x_2",            "Point 2 X"},
    {"property.dest_y_2",            "Point 2 Y"},
    {"property.dest_x_3",            "Point 3 X"},
    {"property.dest_y_3",            "Point 3 Y"},
    {"property.cols",                "Columns"},
    {"property.rows",                "Rows"},
    {"property.curved",              "Curved Interpolation"},
    {"property.mesh_resolution",     "Mesh Resolution"},
    {"property.control_cols",        "Control Columns"},
    {"property.control_rows",        "Control Rows"},
    {"property.center",              "Center"},
    {"property.radius_x",            "Radius X"},
    {"property.radius_y",            "Radius Y"},
    {"property.rotation",            "Rotation"},
    {"property.segments",            "Segments"},
    {"property.closed",              "Closed"},

    // --- Mask (additional) ---
    {"mask.no_alpha_source",         "No alpha texture source"},
    {"mask.space.surface_local",     "Surface Local"},
    {"mask.space.source_uv",         "Source UV"},
    {"mask.space.canvas",            "Canvas"},
    {"mask.space.output",            "Output"},

    // --- Common ---
    {"common.ok",               "OK"},
    {"common.cancel",           "Cancel"},
    {"common.yes",              "Yes"},
    {"common.no",               "No"},
    {"common.warning",          "Warning"},
    {"common.error",            "Error"},
    {"common.enabled",          "Enabled"},
    {"common.disabled",         "Disabled"},
    {"common.language",         "Language"},
    {"common.chinese",          "Chinese"},
    {"common.english",          "English"},
    {"common.auto_detect",      "Auto-detect"},
};

// ===========================================================================
// Built-in Chinese translations
// ===========================================================================
static const MapWrapI18n::TranslationMap sChineseTranslations = {
    // --- 编辑模式 ---
    {"mode.presentation",       "演示模式"},
    {"mode.surface_edit",       "曲面编辑"},
    {"mode.texture_edit",       "纹理编辑"},
    {"mode.source_assign",      "素材分配"},
    {"mode.mask_edit",          "遮罩编辑"},
    {"mode.output_edit",        "输出编辑"},

    // --- 曲面类型 ---
    {"surface.quad",            "四边形"},
    {"surface.grid",            "网格"},
    {"surface.bezier",          "贝塞尔曲面"},
    {"surface.triangle",        "三角形"},
    {"surface.edit_mode",       "编辑模式"},
    {"surface.circle",          "圆形"},
    {"surface.polygon",         "多边形"},

    // --- 曲面操作 ---
    {"surface.add_quad",        "添加四边形"},
    {"surface.add_grid",        "添加网格"},
    {"surface.add_triangle",    "添加三角形"},
    {"surface.add_circle",      "添加圆形"},
    {"surface.add_polygon",     "添加多边形"},
    {"surface.delete",          "删除曲面"},
    {"surface.duplicate",       "复制曲面"},
    {"surface.reset",           "重置曲面"},
    {"surface.flip_h",          "水平翻转"},
    {"surface.flip_v",          "垂直翻转"},
    {"surface.fit_canvas",      "适配画布"},

    // --- 曲面属性 ---
    {"surface.name",            "名称"},
    {"surface.visible",         "可见"},
    {"surface.locked",          "锁定"},
    {"surface.opacity",         "不透明度"},
    {"surface.source",          "素材"},
    {"surface.source_rect",     "素材区域"},
    {"surface.layer",           "图层"},

    // --- 网格 ---
    {"grid.rows",               "行数"},
    {"grid.cols",               "列数"},
    {"grid.resolution",         "分辨率"},
    {"grid.add_row",            "添加行"},
    {"grid.remove_row",         "删除行"},
    {"grid.add_col",            "添加列"},
    {"grid.remove_col",         "删除列"},
    {"grid.interpolation",      "插值方式"},
    {"grid.linear",             "线性"},
    {"grid.curved",             "曲线（Catmull-Rom）"},

    // --- 圆形 ---
    {"circle.center",           "圆心"},
    {"circle.radius_x",         "X 半径"},
    {"circle.radius_y",         "Y 半径"},
    {"circle.rotation",         "旋转"},
    {"circle.segments",         "分段数"},

    // --- 变形 ---
    {"warp.perspective",        "透视"},
    {"warp.grid",               "网格（双线性）"},
    {"warp.bezier",             "贝塞尔曲面"},
    {"warp.perspective_grid",   "透视 + 网格"},
    {"warp.none",               "无"},

    // --- 素材 ---
    {"source.texture",          "纹理"},
    {"source.fbo",              "帧缓冲"},
    {"source.video",            "视频"},
    {"source.none",            "无"},
    {"source.image",            "图片"},
    {"source.generated",        "生成器"},
    {"source.builtin_pattern",  "内置图案"},
    {"source.missing",          "素材缺失"},
    {"source.clock",            "素材时钟"},
    {"source.play",             "播放"},
    {"source.pause",            "暂停"},
    {"source.stop",             "停止"},
    {"source.loop",             "循环"},
    {"source.seek",             "跳转"},

    // --- 校准图案 ---
    {"pattern.checkerboard",    "棋盘格"},
    {"pattern.grid",            "网格"},
    {"pattern.fine_grid",       "细网格"},
    {"pattern.crosshair",       "十字准星"},
    {"pattern.corner_labels",   "角标"},
    {"pattern.uv_gradient",     "UV 渐变"},
    {"pattern.color_bars",      "彩条"},
    {"pattern.luma_ramp",       "亮度渐变"},
    {"pattern.edge_blend_ramp", "边缘融合渐变"},
    {"pattern.alpha_radial",    "Alpha 径向柔边"},
    {"pattern.numbered_cells",  "编号网格"},
    {"pattern.safe_area",       "安全区域"},
    {"pattern.solid_color",     "纯色"},

    // --- 遮罩 ---
    {"mask.rectangle",          "矩形遮罩"},
    {"mask.ellipse",            "椭圆遮罩"},
    {"mask.polygon",            "多边形遮罩"},
    {"mask.bezier",             "贝塞尔遮罩"},
    {"mask.freehand",           "手绘遮罩"},
    {"mask.alpha_texture",      "Alpha 纹理遮罩"},
    {"mask.add",                "添加"},
    {"mask.subtract",           "减去"},
    {"mask.intersect",          "交集"},
    {"mask.inverted",           "反转"},
    {"mask.enabled",            "启用"},
    {"mask.feather",            "羽化"},
    {"mask.opacity",            "不透明度"},
    {"mask.add_mask",           "添加遮罩"},
    {"mask.delete_mask",        "删除遮罩"},

    // --- 输出 / 舞台 ---
    {"output.main",             "主输出"},
    {"output.name",             "输出名称"},
    {"output.enabled",          "输出启用"},
    {"output.canvas_region",    "画布区域"},
    {"output.display_region",   "显示区域"},
    {"output.pixel_size",       "像素尺寸"},
    {"output.content_scale",    "内容缩放"},
    {"output.rotation",         "旋转"},
    {"output.test_pattern",     "显示测试图案"},
    {"output.color_correction", "输出色彩校正"},

    // --- 舞台 ---
    {"stage.design_canvas_size","设计画布尺寸"},
    {"stage.global_masks",      "全局遮罩"},

    // --- 混合 ---
    {"blend.mode",              "混合模式"},
    {"blend.normal",            "正常"},
    {"blend.add",               "叠加"},
    {"blend.multiply",          "正片叠底"},
    {"blend.screen",            "滤色"},
    {"blend.lighten",           "变亮"},
    {"blend.darken",            "变暗"},
    {"blend.alpha_mask",        "Alpha 遮罩"},
    {"blend.brightness",        "亮度"},
    {"blend.edge",              "边缘融合"},
    {"blend.gamma",             "伽马"},
    {"blend.luminance",         "亮度"},
    {"blend.exponent",          "指数"},

    // --- 色彩校正 ---
    {"color.correction",        "色彩校正"},
    {"color.contrast",          "对比度"},
    {"color.saturation",        "饱和度"},
    {"color.lift",              "暗部提升"},
    {"color.gain",              "亮部增益"},
    {"color.black_level",       "黑位"},
    {"color.white_level",       "白位"},
    {"color.premultiplied",     "预乘 Alpha"},

    // --- 编辑器 ---
    {"editor.viewport",         "编辑器视口"},
    {"editor.pan",              "平移"},
    {"editor.zoom",             "缩放"},
    {"editor.fit_view",         "适配视图"},
    {"editor.select",           "选择"},
    {"editor.deselect",         "取消选择"},
    {"editor.nudge",            "微调"},
    {"editor.align_left",       "左对齐"},
    {"editor.align_right",      "右对齐"},
    {"editor.align_top",        "顶对齐"},
    {"editor.align_bottom",     "底对齐"},
    {"editor.align_center_x",   "水平居中"},
    {"editor.align_center_y",   "垂直居中"},
    {"editor.distribute_h",     "水平均分"},
    {"editor.distribute_v",     "垂直均分"},
    {"editor.copy_geometry",    "复制几何"},
    {"editor.paste_geometry",   "粘贴几何"},
    {"editor.copy_uv",          "复制 UV"},
    {"editor.paste_uv",         "粘贴 UV"},

    // --- 吸附 ---
    {"snap.enabled",            "吸附"},
    {"snap.canvas_edges",       "吸附画布边缘"},
    {"snap.canvas_center",      "吸附画布中心"},
    {"snap.other_surfaces",     "吸附其他曲面"},
    {"snap.grid",               "吸附网格"},
    {"snap.grid_step",          "网格步长"},
    {"snap.threshold",          "吸附阈值"},

    // --- 叠加层 ---
    {"overlay.surface_outlines","曲面轮廓"},
    {"overlay.surface_names",   "曲面名称"},
    {"overlay.handles",         "控制手柄"},
    {"overlay.grid_points",     "网格点"},
    {"overlay.mask_points",     "遮罩点"},
    {"overlay.canvas_grid",     "画布网格"},
    {"overlay.canvas_center",   "画布中心"},
    {"overlay.safe_area",       "安全区域"},
    {"overlay.output_bounds",   "输出边界"},
    {"overlay.source_uv",       "素材 UV"},

    // --- 分组 ---
    {"group.name",              "组名"},
    {"group.visible",           "组可见"},
    {"group.locked",            "组锁定"},
    {"group.opacity",           "组不透明度"},
    {"group.translate",         "平移"},
    {"group.rotation",          "旋转"},
    {"group.scale",             "缩放"},
    {"group.add",               "添加组"},
    {"group.delete",            "删除组"},

    // --- 撤销命令 ---
    {"command.delete_surface",       "删除曲面"},
    {"command.create_surface",       "创建曲面"},
    {"command.move_surface",         "移动曲面"},
    {"command.move_vertex",          "移动顶点"},
    {"command.move_uv",              "移动 UV 点"},
    {"command.move_grid",            "移动网格点"},
    {"command.rotate",               "旋转曲面"},
    {"command.edit",                 "编辑曲面"},
    {"command.move_handle",          "移动控制点"},
    {"command.bring_forward",        "上移一层"},
    {"command.send_backward",        "下移一层"},
    {"command.nudge",                "微调曲面"},
    {"command.nudge_handle",         "微调控制点"},
    {"command.fit_to_canvas",        "适配画布"},
    {"command.align_left",           "左对齐"},
    {"command.align_right",          "右对齐"},
    {"command.align_top",            "顶对齐"},
    {"command.align_bottom",         "底对齐"},
    {"command.align_center_x",       "水平居中"},
    {"command.align_center_y",       "垂直居中"},
    {"command.paste_geometry",       "粘贴几何"},
    {"command.paste_uv",             "粘贴 UV"},
    {"command.edit_property",        "编辑属性"},
    {"command.set_handle_position",  "设置控制点位置"},
    {"command.convert_surface",      "转换曲面类型"},
    {"command.add_column",           "增加列"},
    {"command.remove_column",        "减少列"},
    {"command.add_row",              "增加行"},
    {"command.remove_row",           "减少行"},
    {"command.adjust_mesh_resolution","调整网格分辨率"},

    // --- 检查器属性 ---
    {"property.surface_kind",        "曲面类型"},
    {"property.name",                "名称"},
    {"property.visible",             "可见"},
    {"property.locked",              "锁定"},
    {"property.opacity",             "不透明度"},
    {"property.blend_enabled",       "混合启用"},
    {"property.blend_opacity",       "混合不透明度"},
    {"property.blend_brightness",    "混合亮度"},
    {"property.cc_enabled",          "色彩校正启用"},
    {"property.cc_brightness",       "校正亮度"},
    {"property.cc_contrast",         "校正对比度"},
    {"property.cc_saturation",       "校正饱和度"},
    {"property.perspective",         "透视校正"},
    {"property.dest_x_0",            "点 0 X"},
    {"property.dest_y_0",            "点 0 Y"},
    {"property.dest_x_1",            "点 1 X"},
    {"property.dest_y_1",            "点 1 Y"},
    {"property.dest_x_2",            "点 2 X"},
    {"property.dest_y_2",            "点 2 Y"},
    {"property.dest_x_3",            "点 3 X"},
    {"property.dest_y_3",            "点 3 Y"},
    {"property.cols",                "列数"},
    {"property.rows",                "行数"},
    {"property.curved",              "曲线插值"},
    {"property.mesh_resolution",     "网格分辨率"},
    {"property.control_cols",        "控制列数"},
    {"property.control_rows",        "控制行数"},
    {"property.center",              "圆心"},
    {"property.radius_x",            "X 半径"},
    {"property.radius_y",            "Y 半径"},
    {"property.rotation",            "旋转"},
    {"property.segments",            "分段数"},
    {"property.closed",              "闭合"},

    // --- 遮罩（补充） ---
    {"mask.no_alpha_source",         "无 Alpha 纹理素材"},
    {"mask.space.surface_local",     "曲面局部"},
    {"mask.space.source_uv",         "素材 UV"},
    {"mask.space.canvas",            "画布"},
    {"mask.space.output",            "输出"},

    // --- 撤销 / 重做 ---
    {"undo.undo",               "撤销"},
    {"undo.redo",               "重做"},
    {"undo.move_point",         "移动控制点"},
    {"undo.add_surface",        "添加曲面"},
    {"undo.delete_surface",     "删除曲面"},
    {"undo.change_source",      "更换素材"},
    {"undo.change_blend",       "修改混合设置"},
    {"undo.add_mask",           "添加遮罩"},
    {"undo.delete_mask",        "删除遮罩"},

    // --- 项目 ---
    {"project.save",            "保存"},
    {"project.load",            "加载"},
    {"project.validate",        "验证项目"},
    {"project.validation_ok",   "项目验证通过"},
    {"project.missing_source",  "素材缺失"},
    {"project.missing_media",   "媒体文件缺失"},
    {"project.relink",          "重新链接素材"},
    {"project.package",         "打包项目"},

    // --- 几何验证 ---
    {"geometry.valid",              "几何有效"},
    {"geometry.self_intersecting",  "几何自相交"},
    {"geometry.too_small",          "几何面积过小"},
    {"geometry.winding_flipped",    "绕序翻转"},
    {"geometry.has_nan",            "几何包含 NaN 值"},
    {"geometry.repair",             "修复几何"},

    // --- 统计 ---
    {"stats.drawn_surfaces",    "已绘制曲面"},
    {"stats.skipped_surfaces",  "跳过曲面"},
    {"stats.rebuilt_meshes",    "重建网格"},
    {"stats.missing_sources",   "缺失素材"},
    {"stats.invalid_surfaces",  "无效曲面"},
    {"stats.mask_count",        "遮罩"},

    // --- 预设 / 场景 ---
    {"preset.name",             "预设名称"},
    {"cue.name",                "场景名称"},
    {"cue.apply",               "应用场景"},
    {"cue.transition",          "过渡时间（秒）"},

    // --- 通用 ---
    {"common.ok",               "确定"},
    {"common.cancel",           "取消"},
    {"common.yes",              "是"},
    {"common.no",               "否"},
    {"common.warning",          "警告"},
    {"common.error",            "错误"},
    {"common.enabled",          "已启用"},
    {"common.disabled",         "已禁用"},
    {"common.language",         "语言"},
    {"common.chinese",          "中文"},
    {"common.english",          "英文"},
    {"common.auto_detect",      "自动检测"},
};

// ===========================================================================
// MapWrapI18n implementation
// ===========================================================================

MapWrapI18n& MapWrapI18n::instance() {
    static MapWrapI18n sInstance;
    return sInstance;
}

MapWrapI18n::MapWrapI18n()
    : currentLanguage_("en")
    , detectedLanguage_("en")
{
    loadBuiltinTranslations();
}

void MapWrapI18n::loadBuiltinTranslations() {
    enTranslations_ = sEnglishTranslations;
    zhTranslations_ = sChineseTranslations;
}

std::string MapWrapI18n::detectSystemLanguage() {
    // ---------------------------------------------------------------
    // Platform-specific locale detection
    // ---------------------------------------------------------------

#if defined(__APPLE__) && TARGET_OS_IOS
    // iOS: Use NSLocale via Objective-C++
    // We use a simple C approach via CFLocale / NSLocale
    // The Apple platform always returns locale identifiers like:
    //   "zh-Hans", "zh-Hant", "zh_CN", "en_US", "ja_JP", etc.
    @autoreleasepool {
        NSString* lang = [[NSLocale preferredLanguages] firstObject];
        if (lang) {
            std::string localeCode = [lang UTF8String];
            // Check if the primary language is Chinese
            // zh-Hans (Simplified), zh-Hant (Traditional), zh-HK, zh-TW, zh_CN, etc.
            if (localeCode.size() >= 2 && localeCode[0] == 'z' && localeCode[1] == 'h') {
                return "zh";
            }
        }
    }
    return "en";

#elif defined(__APPLE__) && (TARGET_OS_OSX || (TARGET_OS_MAC && !TARGET_OS_IPHONE))
    // macOS: Use NSLocale via Objective-C++
    @autoreleasepool {
        NSString* lang = [[NSLocale preferredLanguages] firstObject];
        if (lang) {
            std::string localeCode = [lang UTF8String];
            if (localeCode.size() >= 2 && localeCode[0] == 'z' && localeCode[1] == 'h') {
                return "zh";
            }
        }
    }
    return "en";

#elif defined(_WIN32)
    // Windows: Use GetUserDefaultUILanguage or Win32 locale API
    // LANG_CHINESE = 0x04
    LANGID langId = GetUserDefaultUILanguage();
    PRIMARYLANGID primaryLang = PRIMARYLANGID(langId);
    if (primaryLang == 0x04) {  // LANG_CHINESE
        return "zh";
    }
    return "en";

#else
    // Fallback: check environment variables
    const char* lang = std::getenv("LANG");
    if (lang) {
        std::string langStr(lang);
        if (langStr.size() >= 2 && langStr[0] == 'z' && langStr[1] == 'h') {
            return "zh";
        }
    }
    const char* lcAll = std::getenv("LC_ALL");
    if (lcAll) {
        std::string lcStr(lcAll);
        if (lcStr.size() >= 2 && lcStr[0] == 'z' && lcStr[1] == 'h') {
            return "zh";
        }
    }
    return "en";
#endif
}

void MapWrapI18n::detectAndSetLanguage() {
    std::lock_guard<std::mutex> lock(mutex_);
    detectedLanguage_ = detectSystemLanguage();
    std::string oldLang = currentLanguage_;
    currentLanguage_ = detectedLanguage_;

    // Notify callbacks if language changed
    if (oldLang != currentLanguage_) {
        for (auto& cb : languageChangeCallbacks_) {
            cb(currentLanguage_);
        }
    }
}

void MapWrapI18n::setLanguage(const std::string& langCode) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string oldLang = currentLanguage_;
    currentLanguage_ = langCode;

    if (oldLang != currentLanguage_) {
        for (auto& cb : languageChangeCallbacks_) {
            cb(currentLanguage_);
        }
    }
}

const std::string& MapWrapI18n::language() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return currentLanguage_;
}

bool MapWrapI18n::isChinese() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return currentLanguage_ == "zh";
}

const std::string& MapWrapI18n::tr(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);

    // 1. Try current language
    const TranslationMap* primaryMap = nullptr;
    if (currentLanguage_ == "zh") {
        primaryMap = &zhTranslations_;
    } else if (currentLanguage_ == "en") {
        primaryMap = &enTranslations_;
    } else {
        // Check custom translations
        auto it = customTranslations_.find(currentLanguage_);
        if (it != customTranslations_.end()) {
            auto keyIt = it->second.find(key);
            if (keyIt != it->second.end()) {
                return keyIt->second;
            }
        }
    }

    if (primaryMap) {
        auto it = primaryMap->find(key);
        if (it != primaryMap->end()) {
            return it->second;
        }

        // Check custom translations for current language
        auto custIt = customTranslations_.find(currentLanguage_);
        if (custIt != customTranslations_.end()) {
            auto keyIt = custIt->second.find(key);
            if (keyIt != custIt->second.end()) {
                return keyIt->second;
            }
        }
    }

    // 2. Fallback to English
    auto enIt = enTranslations_.find(key);
    if (enIt != enTranslations_.end()) {
        return enIt->second;
    }

    // 3. Check custom English
    auto custEnIt = customTranslations_.find("en");
    if (custEnIt != customTranslations_.end()) {
        auto keyIt = custEnIt->second.find(key);
        if (keyIt != custEnIt->second.end()) {
            return keyIt->second;
        }
    }

    // 4. Last resort: return the key itself
    // We need a static string that persists — use a thread-local static
    // to avoid issues with concurrent access to a single static.
    // Since this is a fallback, it's acceptable.
    static thread_local std::string sFallbackKey;
    sFallbackKey = key;
    return sFallbackKey;
}

bool MapWrapI18n::hasTranslation(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);

    const TranslationMap* primaryMap = nullptr;
    if (currentLanguage_ == "zh") {
        primaryMap = &zhTranslations_;
    } else if (currentLanguage_ == "en") {
        primaryMap = &enTranslations_;
    } else {
        auto it = customTranslations_.find(currentLanguage_);
        if (it != customTranslations_.end()) {
            return it->second.find(key) != it->second.end();
        }
        return false;
    }

    if (primaryMap && primaryMap->find(key) != primaryMap->end()) {
        return true;
    }

    auto custIt = customTranslations_.find(currentLanguage_);
    if (custIt != customTranslations_.end()) {
        return custIt->second.find(key) != custIt->second.end();
    }

    return false;
}

void MapWrapI18n::addTranslations(const std::string& langCode, const TranslationMap& entries) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& map = customTranslations_[langCode];
    for (const auto& [k, v] : entries) {
        map[k] = v;
    }
}

std::vector<std::string> MapWrapI18n::availableLanguages() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> langs;
    langs.push_back("en");
    if (zhTranslations_.size() > 0 && std::find(langs.begin(), langs.end(), "zh") == langs.end()) {
        langs.push_back("zh");
    }
    for (const auto& [code, _] : customTranslations_) {
        if (std::find(langs.begin(), langs.end(), code) == langs.end()) {
            langs.push_back(code);
        }
    }
    return langs;
}

void MapWrapI18n::resetToDetected() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string oldLang = currentLanguage_;
    currentLanguage_ = detectedLanguage_;
    if (oldLang != currentLanguage_) {
        for (auto& cb : languageChangeCallbacks_) {
            cb(currentLanguage_);
        }
    }
}

void MapWrapI18n::onLanguageChange(LanguageChangeCallback cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    languageChangeCallbacks_.push_back(std::move(cb));
}

} // namespace mapwrap
} // namespace tcx
