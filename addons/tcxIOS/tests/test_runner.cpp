#include "test_common.h"

extern void test_public_include_compiles();
extern void test_public_headers_have_no_native_imports();
extern void test_configuration_requirements_cover_privacy_and_capabilities();
extern void test_logger_dispatches_records_to_handler();
extern void test_event_queue_posts_and_drains();
extern void test_scene_context_roundtrip();
extern void test_single_shot_completion_only_invokes_first_result();
extern void test_operation_cancel_removes_active_operation_and_runs_handler_once();
extern void test_cancellable_completion_drops_result_after_cancel_and_cleans_up();
extern void test_feature_manifest_contains_v0_permissions();
extern void test_feature_manifest_reports_missing_keys();
extern void test_feature_manifest_does_not_require_notification_usage_description();
extern void test_feature_manifest_accepts_vision_without_app_capabilities();
extern void test_stub_app_device_defaults();
extern void test_stub_async_operations_return_unavailable();
extern void test_stub_sync_features_return_false();

int main() {
    std::vector<TestCase> tests = {
        {"public_include_compiles", test_public_include_compiles},
        {"public_headers_have_no_native_imports", test_public_headers_have_no_native_imports},
        {"configuration_requirements_cover_privacy_and_capabilities", test_configuration_requirements_cover_privacy_and_capabilities},
        {"logger_dispatches_records_to_handler", test_logger_dispatches_records_to_handler},
        {"event_queue_posts_and_drains", test_event_queue_posts_and_drains},
        {"scene_context_roundtrip", test_scene_context_roundtrip},
        {"single_shot_completion_only_invokes_first_result", test_single_shot_completion_only_invokes_first_result},
        {"operation_cancel_removes_active_operation_and_runs_handler_once", test_operation_cancel_removes_active_operation_and_runs_handler_once},
        {"cancellable_completion_drops_result_after_cancel_and_cleans_up", test_cancellable_completion_drops_result_after_cancel_and_cleans_up},
        {"feature_manifest_contains_v0_permissions", test_feature_manifest_contains_v0_permissions},
        {"feature_manifest_reports_missing_keys", test_feature_manifest_reports_missing_keys},
        {"feature_manifest_does_not_require_notification_usage_description", test_feature_manifest_does_not_require_notification_usage_description},
        {"feature_manifest_accepts_vision_without_app_capabilities", test_feature_manifest_accepts_vision_without_app_capabilities},
        {"stub_app_device_defaults", test_stub_app_device_defaults},
        {"stub_async_operations_return_unavailable", test_stub_async_operations_return_unavailable},
        {"stub_sync_features_return_false", test_stub_sync_features_return_false}
    };

    int failed = 0;
    for (const auto& test : tests) {
        try {
            test.func();
            std::cout << "[PASS] " << test.name << "\n";
        } catch (const std::exception& e) {
            ++failed;
            std::cerr << "[FAIL] " << test.name << ": " << e.what() << "\n";
        }
    }

    if (failed != 0) {
        std::cerr << failed << " test(s) failed\n";
        return 1;
    }

    std::cout << tests.size() << " test(s) passed\n";
    return 0;
}
