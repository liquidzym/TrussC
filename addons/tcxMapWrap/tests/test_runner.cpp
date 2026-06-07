// =============================================================================
// tcxMapWrap — Test Runner
// =============================================================================
// Includes all test files and runs them, printing pass/fail.

#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <chrono>

// --------------------------------------------------------------------------
// Test macro definitions (same as in each test file, repeated here for
// the runner's own use if needed; each .cpp also defines its own copy)
// --------------------------------------------------------------------------
#define TEST(name) void test_##name()
#define ASSERT_TRUE(cond) do { if(!(cond)) throw std::runtime_error("ASSERT_TRUE failed: " #cond); } while(0)
#define ASSERT_EQ(a,b) do { if((a)!=(b)) throw std::runtime_error("ASSERT_EQ failed"); } while(0)
#define ASSERT_NEAR(a,b,eps) do { if(std::fabs((a)-(b))>(eps)) throw std::runtime_error("ASSERT_NEAR failed"); } while(0)

// --------------------------------------------------------------------------
// Forward declarations of all test functions
// --------------------------------------------------------------------------

// test_homography.cpp
extern void test_identity_transform();
extern void test_scale_transform();
extern void test_translate_transform();
extern void test_arbitrary_convex_quad();
extern void test_inverse_transform();
extern void test_near_degenerate_quad();
extern void test_self_intersecting_quad();
extern void test_all_zeros();

// test_grid_warp.cpp
extern void test_grid_2x2_bilinear();
extern void test_grid_3x3_center_move();
extern void test_add_column_preserves_boundary();
extern void test_remove_row_doesnt_crash();
extern void test_catmull_rom_boundary_stability();
extern void test_resolution_change_index_count();
extern void test_quad_default_mesh_resolution_subdivides();
extern void test_bezier_surface_mesh_counts();
extern void test_bezier_surface_control_point_hit();
extern void test_bezier_surface_evaluates_corners();
extern void test_editor_select_adjacent_grid_handle_wraps();
extern void test_editor_lattice_resize_controls();
extern void test_editor_convert_quad_to_bezier_preserves_common_state();

// test_serialization.cpp
extern void test_empty_document_roundtrip();
extern void test_document_create_requires_explicit_add();
extern void test_one_quad_surface_roundtrip();
extern void test_mixed_surfaces_roundtrip();
extern void test_mixed_masks_roundtrip();
extern void test_outputs_roundtrip();
extern void test_groups_roundtrip();
extern void test_missing_optional_fields_defaults();
extern void test_unknown_fields_ignored();
extern void test_source_missing_loads_with_warning();
extern void test_old_json_without_stage();
extern void test_source_registry_roundtrip();
extern void test_load_replaces_existing_document();
extern void test_grid_all_control_points_roundtrip();
extern void test_bezier_surface_roundtrip();
extern void test_surface_bad_field_loads_with_warning_without_dropping_file();
extern void test_malformed_field_type_returns_load_error();
extern void test_stage_empty_outputs_restores_default_output();
extern void test_source_registry_load_without_sources_clears_existing_registry();

// test_hit_test.cpp
extern void test_vertex_hit_on_quad();
extern void test_edge_hit_on_quad();
extern void test_body_hit_on_quad();
extern void test_mask_point_hit();
extern void test_mask_edge_hit();
extern void test_locked_surface_ignored_by_editor_hit();
extern void test_top_layer_priority();
extern void test_touch_hit_radius_larger_than_mouse();
extern void test_zoom_touch_radius_screen_pixels();

// test_undo.cpp
extern void test_move_point_undo_redo();
extern void test_add_surface_undo_redo();
extern void test_delete_surface_undo_redo();
extern void test_source_assignment_undo_redo();
extern void test_mask_point_undo_redo();
extern void test_add_delete_mask_undo_redo();
extern void test_drag_merged_into_single_command();
extern void test_cant_undo_empty_stack();
extern void test_cant_redo_after_new_push();
extern void test_push_already_executed_does_not_execute_twice();

// test_masks.cpp
extern void test_polygon_mask_json_roundtrip();
extern void test_ellipse_mask_json_roundtrip();
extern void test_inverted_mask_flag();
extern void test_mask_hit_test_vertex();
extern void test_mask_hit_test_edge();
extern void test_subtract_mask_doesnt_crash();
extern void test_missing_alpha_texture_warning();

// test_polygon_surface.cpp
extern void test_convex_polygon_triangulation();
extern void test_concave_polygon_triangulation();
extern void test_self_intersecting_polygon_invalid();
extern void test_point_add_remove_undo_redo();
extern void test_source_rect_uv_defaults();
extern void test_save_load_roundtrip();
extern void test_polygon_build_mesh_uses_saved_uv_points();
extern void test_polygon_default_uvs_are_local_bounds_not_canvas_points();

// test_outputs.cpp
extern void test_default_output_auto_created();
extern void test_output_canvas_region_save_load();
extern void test_disabled_output_doesnt_draw();
extern void test_output_color_correction_field_persistence();
extern void test_old_json_without_stage_readable();
extern void test_renderer_mesh_rebuilds_after_surface_revision_change();
extern void test_undo_move_control_point_marks_surface_for_renderer();
extern void test_engine_updates_generated_sources_once_per_frame();
extern void test_editor_drag_uses_engine_canvas_size();
extern void test_renderer_feathered_ellipse_mask_has_partial_alpha();
extern void test_renderer_complex_mask_subtract_reduces_alpha();
extern void test_renderer_masked_quad_uses_subdivision_for_mask_coverage();
extern void test_renderer_uses_primary_output_masks_without_mixing_other_outputs();

// test_calibration_patterns.cpp
extern void test_checkerboard_source_generation();
extern void test_grid_source_generation();
extern void test_uv_gradient_source_generation();
extern void test_pattern_size_correct();
extern void test_alpha_radial_contains_soft_alpha();
extern void test_pattern_name_returns_localized_string();

// test_project_validation.cpp
extern void test_existing_image_path_ok();
extern void test_missing_image_path_warning();
extern void test_missing_video_path_warning();
extern void test_relative_path_resolution();
extern void test_source_missing_surface_not_crash();

// test_editor_viewport.cpp
extern void test_viewport_identity_transform();
extern void test_fit_canvas_to_view();
extern void test_pan();
extern void test_zoom_at_cursor();
extern void test_screen_to_canvas_roundtrip();
extern void test_touch_radius_independent_of_zoom();

// test_geometry_validation.cpp
extern void test_valid_quad();
extern void test_geom_self_intersecting_quad();
extern void test_tiny_quad();
extern void test_flipped_winding();
extern void test_nan_point();
extern void test_repair_winding();

// test_color_correction.cpp
extern void test_default_values_serialize_deserialize();
extern void test_source_correction_serialize();
extern void test_surface_correction_serialize();
extern void test_output_correction_serialize();

// test_i18n.cpp
extern void test_default_language_is_en();
extern void test_set_language_zh();
extern void test_set_language_en();
extern void test_is_chinese_returns_true_for_zh();
extern void test_tr_surface_quad_returns_quad_in_english();
extern void test_tr_surface_quad_returns_chinese_in_zh();
extern void test_tr_nonexistent_key_returns_key();
extern void test_has_translation_returns_true_for_existing();
extern void test_has_translation_returns_false_for_missing();
extern void test_add_translations_adds_custom_entries();
extern void test_available_languages_includes_en_zh();
extern void test_reset_to_detected_reverts();
extern void test_on_language_change_callback_fires();

// test_api_regressions.cpp
extern void test_surface_mutable_geometry_access_marks_dirty();
extern void test_surface_clone_deep_copies_common_and_geometry_state();
extern void test_warp_clone_deep_copies_state();
extern void test_delete_surface_undo_restores_snapshot_clone();
extern void test_polygon_add_remove_keeps_uv_points_in_sync();
extern void test_circle_segments_clamped_to_supported_range();
extern void test_mat3_helpers_multiply_transform_and_compare();
extern void test_pointer_event_factories_allow_non_down_types();
extern void test_document_const_get_surface_returns_const_pointer();
extern void test_generated_source_pixel_callback_writes_output_buffer();
extern void test_document_reorder_surface_uses_final_index();
extern void test_grid_constructor_clamps_and_bounds_checks_points();
extern void test_quad_homography_fallback_preserves_perspective_setting();
extern void test_autosave_preserves_document_dirty_flag();
extern void test_masked_surface_draw_does_not_dirty_or_rebuild_each_frame();
extern void test_quad_perspective_uv_points_do_not_change_destination_geometry();
extern void test_editor_set_selected_property_records_already_applied_undo();
extern void test_editor_duplicate_selected_uses_document_kind_id_sequence();

// --------------------------------------------------------------------------
// Test registry
// --------------------------------------------------------------------------
struct TestCase {
    const char* name;
    void (*func)();
};

static std::vector<TestCase> allTests = {
    // test_homography.cpp
    {"homography::identity_transform",            test_identity_transform},
    {"homography::scale_transform",               test_scale_transform},
    {"homography::translate_transform",           test_translate_transform},
    {"homography::arbitrary_convex_quad",          test_arbitrary_convex_quad},
    {"homography::inverse_transform",             test_inverse_transform},
    {"homography::near_degenerate_quad",          test_near_degenerate_quad},
    {"homography::self_intersecting_quad",        test_self_intersecting_quad},
    {"homography::all_zeros",                     test_all_zeros},

    // test_grid_warp.cpp
    {"grid_warp::grid_2x2_bilinear",              test_grid_2x2_bilinear},
    {"grid_warp::grid_3x3_center_move",           test_grid_3x3_center_move},
    {"grid_warp::add_column_preserves_boundary",  test_add_column_preserves_boundary},
    {"grid_warp::remove_row_doesnt_crash",        test_remove_row_doesnt_crash},
    {"grid_warp::catmull_rom_boundary_stability", test_catmull_rom_boundary_stability},
    {"grid_warp::resolution_change_index_count",  test_resolution_change_index_count},
    {"quad_warp::default_mesh_resolution_subdivides", test_quad_default_mesh_resolution_subdivides},
    {"bezier_surface::mesh_counts",               test_bezier_surface_mesh_counts},
    {"bezier_surface::control_point_hit",         test_bezier_surface_control_point_hit},
    {"bezier_surface::evaluates_corners",         test_bezier_surface_evaluates_corners},
    {"editor::select_adjacent_grid_handle_wraps", test_editor_select_adjacent_grid_handle_wraps},
    {"editor::lattice_resize_controls",           test_editor_lattice_resize_controls},
    {"editor::convert_quad_to_bezier_preserves_common_state", test_editor_convert_quad_to_bezier_preserves_common_state},

    // test_serialization.cpp
    {"serialization::empty_document_roundtrip",       test_empty_document_roundtrip},
    {"serialization::document_create_requires_explicit_add", test_document_create_requires_explicit_add},
    {"serialization::one_quad_surface_roundtrip",     test_one_quad_surface_roundtrip},
    {"serialization::mixed_surfaces_roundtrip",       test_mixed_surfaces_roundtrip},
    {"serialization::mixed_masks_roundtrip",          test_mixed_masks_roundtrip},
    {"serialization::outputs_roundtrip",              test_outputs_roundtrip},
    {"serialization::groups_roundtrip",               test_groups_roundtrip},
    {"serialization::missing_optional_fields_defaults", test_missing_optional_fields_defaults},
    {"serialization::unknown_fields_ignored",         test_unknown_fields_ignored},
    {"serialization::source_missing_loads_with_warning", test_source_missing_loads_with_warning},
    {"serialization::old_json_without_stage",         test_old_json_without_stage},
    {"serialization::source_registry_roundtrip",      test_source_registry_roundtrip},
    {"serialization::load_replaces_existing_document", test_load_replaces_existing_document},
    {"serialization::grid_all_control_points_roundtrip", test_grid_all_control_points_roundtrip},
    {"serialization::bezier_surface_roundtrip",   test_bezier_surface_roundtrip},
    {"serialization::surface_bad_field_loads_with_warning", test_surface_bad_field_loads_with_warning_without_dropping_file},
    {"serialization::malformed_field_type_returns_load_error", test_malformed_field_type_returns_load_error},
    {"serialization::stage_empty_outputs_restores_default_output", test_stage_empty_outputs_restores_default_output},
    {"serialization::source_registry_load_without_sources_clears_existing_registry", test_source_registry_load_without_sources_clears_existing_registry},

    // test_hit_test.cpp
    {"hit_test::vertex_hit_on_quad",               test_vertex_hit_on_quad},
    {"hit_test::edge_hit_on_quad",                 test_edge_hit_on_quad},
    {"hit_test::body_hit_on_quad",                 test_body_hit_on_quad},
    {"hit_test::mask_point_hit",                   test_mask_point_hit},
    {"hit_test::mask_edge_hit",                    test_mask_edge_hit},
    {"hit_test::locked_surface_ignored",           test_locked_surface_ignored_by_editor_hit},
    {"hit_test::top_layer_priority",               test_top_layer_priority},
    {"hit_test::touch_hit_radius_larger_than_mouse", test_touch_hit_radius_larger_than_mouse},
    {"hit_test::zoom_touch_radius_screen_pixels",  test_zoom_touch_radius_screen_pixels},

    // test_undo.cpp
    {"undo::move_point_undo_redo",                 test_move_point_undo_redo},
    {"undo::add_surface_undo_redo",                test_add_surface_undo_redo},
    {"undo::delete_surface_undo_redo",             test_delete_surface_undo_redo},
    {"undo::source_assignment_undo_redo",          test_source_assignment_undo_redo},
    {"undo::mask_point_undo_redo",                 test_mask_point_undo_redo},
    {"undo::add_delete_mask_undo_redo",            test_add_delete_mask_undo_redo},
    {"undo::drag_merged_into_single_command",      test_drag_merged_into_single_command},
    {"undo::cant_undo_empty_stack",                test_cant_undo_empty_stack},
    {"undo::cant_redo_after_new_push",             test_cant_redo_after_new_push},
    {"undo::push_already_executed_does_not_execute_twice", test_push_already_executed_does_not_execute_twice},

    // test_masks.cpp
    {"masks::polygon_mask_json_roundtrip",         test_polygon_mask_json_roundtrip},
    {"masks::ellipse_mask_json_roundtrip",         test_ellipse_mask_json_roundtrip},
    {"masks::inverted_mask_flag",                  test_inverted_mask_flag},
    {"masks::mask_hit_test_vertex",               test_mask_hit_test_vertex},
    {"masks::mask_hit_test_edge",                 test_mask_hit_test_edge},
    {"masks::subtract_mask_doesnt_crash",         test_subtract_mask_doesnt_crash},
    {"masks::missing_alpha_texture_warning",      test_missing_alpha_texture_warning},

    // test_polygon_surface.cpp
    {"polygon::convex_polygon_triangulation",      test_convex_polygon_triangulation},
    {"polygon::concave_polygon_triangulation",     test_concave_polygon_triangulation},
    {"polygon::self_intersecting_polygon_invalid", test_self_intersecting_polygon_invalid},
    {"polygon::point_add_remove_undo_redo",        test_point_add_remove_undo_redo},
    {"polygon::source_rect_uv_defaults",          test_source_rect_uv_defaults},
    {"polygon::save_load_roundtrip",              test_save_load_roundtrip},
    {"polygon::build_mesh_uses_saved_uv_points",  test_polygon_build_mesh_uses_saved_uv_points},
    {"polygon::default_uvs_are_local_bounds",     test_polygon_default_uvs_are_local_bounds_not_canvas_points},

    // test_outputs.cpp
    {"outputs::default_output_auto_created",       test_default_output_auto_created},
    {"outputs::output_canvas_region_save_load",    test_output_canvas_region_save_load},
    {"outputs::disabled_output_doesnt_draw",       test_disabled_output_doesnt_draw},
    {"outputs::output_color_correction_persistence", test_output_color_correction_field_persistence},
    {"outputs::old_json_without_stage_readable",   test_old_json_without_stage_readable},
    {"outputs::renderer_mesh_rebuilds_after_surface_revision_change", test_renderer_mesh_rebuilds_after_surface_revision_change},
    {"outputs::undo_move_control_point_marks_surface_for_renderer", test_undo_move_control_point_marks_surface_for_renderer},
    {"outputs::engine_updates_generated_sources_once_per_frame", test_engine_updates_generated_sources_once_per_frame},
    {"outputs::editor_drag_uses_engine_canvas_size", test_editor_drag_uses_engine_canvas_size},
    {"outputs::renderer_feathered_ellipse_mask_has_partial_alpha", test_renderer_feathered_ellipse_mask_has_partial_alpha},
    {"outputs::renderer_complex_mask_subtract_reduces_alpha", test_renderer_complex_mask_subtract_reduces_alpha},
    {"outputs::renderer_masked_quad_uses_subdivision_for_mask_coverage", test_renderer_masked_quad_uses_subdivision_for_mask_coverage},
    {"outputs::renderer_uses_primary_output_masks_without_mixing_other_outputs", test_renderer_uses_primary_output_masks_without_mixing_other_outputs},

    // test_calibration_patterns.cpp
    {"calibration::checkerboard_source_generation",  test_checkerboard_source_generation},
    {"calibration::grid_source_generation",          test_grid_source_generation},
    {"calibration::uv_gradient_source_generation",   test_uv_gradient_source_generation},
    {"calibration::pattern_size_correct",            test_pattern_size_correct},
    {"calibration::alpha_radial_contains_soft_alpha", test_alpha_radial_contains_soft_alpha},
    {"calibration::pattern_name_localized",          test_pattern_name_returns_localized_string},

    // test_project_validation.cpp
    {"validation::existing_image_path_ok",          test_existing_image_path_ok},
    {"validation::missing_image_path_warning",      test_missing_image_path_warning},
    {"validation::missing_video_path_warning",      test_missing_video_path_warning},
    {"validation::relative_path_resolution",        test_relative_path_resolution},
    {"validation::source_missing_surface_not_crash", test_source_missing_surface_not_crash},

    // test_editor_viewport.cpp
    {"viewport::identity_transform",               test_viewport_identity_transform},
    {"viewport::fit_canvas_to_view",               test_fit_canvas_to_view},
    {"viewport::pan",                              test_pan},
    {"viewport::zoom_at_cursor",                   test_zoom_at_cursor},
    {"viewport::screen_to_canvas_roundtrip",       test_screen_to_canvas_roundtrip},
    {"viewport::touch_radius_independent_of_zoom", test_touch_radius_independent_of_zoom},

    // test_geometry_validation.cpp
    {"geometry::valid_quad",                       test_valid_quad},
    {"geometry::self_intersecting_quad",           test_geom_self_intersecting_quad},
    {"geometry::tiny_quad",                        test_tiny_quad},
    {"geometry::flipped_winding",                  test_flipped_winding},
    {"geometry::nan_point",                        test_nan_point},
    {"geometry::repair_winding",                   test_repair_winding},

    // test_color_correction.cpp
    {"color_correction::default_values_serialize_deserialize", test_default_values_serialize_deserialize},
    {"color_correction::source_correction_serialize",          test_source_correction_serialize},
    {"color_correction::surface_correction_serialize",         test_surface_correction_serialize},
    {"color_correction::output_correction_serialize",          test_output_correction_serialize},

    // test_i18n.cpp
    {"i18n::default_language_is_en",                     test_default_language_is_en},
    {"i18n::set_language_zh",                            test_set_language_zh},
    {"i18n::set_language_en",                            test_set_language_en},
    {"i18n::is_chinese_returns_true_for_zh",             test_is_chinese_returns_true_for_zh},
    {"i18n::tr_surface_quad_returns_quad_in_english",    test_tr_surface_quad_returns_quad_in_english},
    {"i18n::tr_surface_quad_returns_chinese_in_zh",      test_tr_surface_quad_returns_chinese_in_zh},
    {"i18n::tr_nonexistent_key_returns_key",             test_tr_nonexistent_key_returns_key},
    {"i18n::has_translation_returns_true_for_existing",  test_has_translation_returns_true_for_existing},
    {"i18n::has_translation_returns_false_for_missing",  test_has_translation_returns_false_for_missing},
    {"i18n::add_translations_adds_custom_entries",       test_add_translations_adds_custom_entries},
    {"i18n::available_languages_includes_en_zh",         test_available_languages_includes_en_zh},
    {"i18n::reset_to_detected_reverts",                  test_reset_to_detected_reverts},
    {"i18n::on_language_change_callback_fires",          test_on_language_change_callback_fires},

    // test_api_regressions.cpp
    {"api::surface_mutable_geometry_access_marks_dirty", test_surface_mutable_geometry_access_marks_dirty},
    {"api::surface_clone_deep_copies_common_and_geometry_state", test_surface_clone_deep_copies_common_and_geometry_state},
    {"api::warp_clone_deep_copies_state",                test_warp_clone_deep_copies_state},
    {"api::delete_surface_undo_restores_snapshot_clone", test_delete_surface_undo_restores_snapshot_clone},
    {"api::polygon_add_remove_keeps_uv_points_in_sync",  test_polygon_add_remove_keeps_uv_points_in_sync},
    {"api::circle_segments_clamped_to_supported_range",  test_circle_segments_clamped_to_supported_range},
    {"api::mat3_helpers_multiply_transform_and_compare", test_mat3_helpers_multiply_transform_and_compare},
    {"api::pointer_event_factories_allow_non_down_types", test_pointer_event_factories_allow_non_down_types},
    {"api::document_const_get_surface_returns_const_pointer", test_document_const_get_surface_returns_const_pointer},
    {"api::generated_source_pixel_callback_writes_output_buffer", test_generated_source_pixel_callback_writes_output_buffer},
    {"api::document_reorder_surface_uses_final_index", test_document_reorder_surface_uses_final_index},
    {"api::grid_constructor_clamps_and_bounds_checks_points", test_grid_constructor_clamps_and_bounds_checks_points},
    {"api::quad_homography_fallback_preserves_perspective_setting", test_quad_homography_fallback_preserves_perspective_setting},
    {"api::autosave_preserves_document_dirty_flag", test_autosave_preserves_document_dirty_flag},
    {"api::masked_surface_draw_does_not_dirty_or_rebuild_each_frame", test_masked_surface_draw_does_not_dirty_or_rebuild_each_frame},
    {"api::quad_perspective_uv_points_do_not_change_destination_geometry", test_quad_perspective_uv_points_do_not_change_destination_geometry},
    {"api::editor_set_selected_property_records_already_applied_undo", test_editor_set_selected_property_records_already_applied_undo},
    {"api::editor_duplicate_selected_uses_document_kind_id_sequence", test_editor_duplicate_selected_uses_document_kind_id_sequence},
};

// --------------------------------------------------------------------------
// main
// --------------------------------------------------------------------------
int main() {
    int passed = 0;
    int failed = 0;
    std::vector<std::string> failures;

    std::cout << "================================================================" << std::endl;
    std::cout << "  tcxMapWrap Test Suite" << std::endl;
    std::cout << "  " << allTests.size() << " tests" << std::endl;
    std::cout << "================================================================" << std::endl;
    std::cout << std::endl;

    for (const auto& tc : allTests) {
        std::cout << "  [RUN ] " << tc.name << std::flush;
        try {
            tc.func();
            std::cout << "  [PASS]" << std::endl;
            passed++;
        } catch (const std::exception& e) {
            std::cout << "  [FAIL]" << std::endl;
            std::cout << "         Error: " << e.what() << std::endl;
            failed++;
            failures.push_back(std::string(tc.name) + ": " + e.what());
        }
    }

    std::cout << std::endl;
    std::cout << "================================================================" << std::endl;
    std::cout << "  Results: " << passed << " passed, " << failed << " failed, "
              << (passed + failed) << " total" << std::endl;
    std::cout << "================================================================" << std::endl;

    if (failed > 0) {
        std::cout << std::endl << "  Failed tests:" << std::endl;
        for (const auto& f : failures) {
            std::cout << "    - " << f << std::endl;
        }
        std::cout << std::endl;
    }

    return failed > 0 ? 1 : 0;
}
