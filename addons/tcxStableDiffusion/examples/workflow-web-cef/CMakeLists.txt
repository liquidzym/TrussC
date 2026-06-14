cmake_minimum_required(VERSION 3.20)
project(workflow-web-cef)

if(NOT DEFINED TRUSSC_DIR)
    set(TRUSSC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../core")
endif()

include(${TRUSSC_DIR}/cmake/trussc_app.cmake)

trussc_app()
tcxcef_copy_runtime_files(workflow-web-cef)

set(WORKFLOW_WEB_CEF_WEB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/web")
set(WORKFLOW_WEB_CEF_WORKER_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/worker")
set(WORKFLOW_WEB_CEF_WORKFLOWS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/workflows")
set(WORKFLOW_WEB_CEF_RUNTIME_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/runtime")
set(WORKFLOW_WEB_CEF_NODE_PACKAGE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../node")
set(WORKFLOW_WEB_CEF_DATA_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/bin/data")

if(NOT EXISTS "${WORKFLOW_WEB_CEF_WEB_ROOT}/dist/index.html")
    message(FATAL_ERROR
        "[workflow-web-cef] web assets are missing. Run from examples/workflow-web-cef/web:\n"
        "  npm install\n"
        "  npm run build")
endif()

if(NOT EXISTS "${WORKFLOW_WEB_CEF_WORKER_ROOT}/dist/worker.mjs")
    message(FATAL_ERROR
        "[workflow-web-cef] worker bundle is missing. Run from examples/workflow-web-cef/worker:\n"
        "  npm install\n"
        "  npm run build")
endif()

if(WIN32 AND NOT EXISTS "${WORKFLOW_WEB_CEF_RUNTIME_ROOT}/node/node.exe")
    message(FATAL_ERROR
        "[workflow-web-cef] portable Node runtime is missing. Run:\n"
        "  python ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/prepare_workflow_web_cef_assets.py --node-runtime")
endif()

if(NOT EXISTS "${WORKFLOW_WEB_CEF_DATA_ROOT}/models")
    message(FATAL_ERROR
        "[workflow-web-cef] bin/data models are missing. Run:\n"
        "  python ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/prepare_workflow_web_cef_assets.py --all")
endif()

add_custom_target(workflow_web_cef_assets ALL
    COMMAND ${CMAKE_COMMAND} -E remove_directory
        "$<TARGET_FILE_DIR:workflow-web-cef>/workflow-web-cef"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:workflow-web-cef>/workflow-web-cef/web"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:workflow-web-cef>/workflow-web-cef/worker"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:workflow-web-cef>/workflow-web-cef/workflows"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:workflow-web-cef>/runtime"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:workflow-web-cef>/data"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${WORKFLOW_WEB_CEF_WEB_ROOT}/dist"
        "$<TARGET_FILE_DIR:workflow-web-cef>/workflow-web-cef/web/dist"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${WORKFLOW_WEB_CEF_WORKER_ROOT}/dist"
        "$<TARGET_FILE_DIR:workflow-web-cef>/workflow-web-cef/worker/dist"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${WORKFLOW_WEB_CEF_WORKFLOWS_ROOT}"
        "$<TARGET_FILE_DIR:workflow-web-cef>/workflow-web-cef/workflows"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${WORKFLOW_WEB_CEF_RUNTIME_ROOT}"
        "$<TARGET_FILE_DIR:workflow-web-cef>/runtime"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${WORKFLOW_WEB_CEF_NODE_PACKAGE_ROOT}"
        "$<TARGET_FILE_DIR:workflow-web-cef>/runtime/node-package"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${WORKFLOW_WEB_CEF_DATA_ROOT}"
        "$<TARGET_FILE_DIR:workflow-web-cef>/data"
    COMMENT "[workflow-web-cef] Copying web, worker, workflows, runtime/node, node package, and bin/data assets"
    VERBATIM)
add_dependencies(workflow_web_cef_assets workflow-web-cef)
