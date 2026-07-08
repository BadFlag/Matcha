/**
 * @file NyanCadApp.cpp
 * @brief NyanCad demo application implementation.
 */

#include "NyanCadApp.h"

#include "MeshEditors/NyanFwSketchEditor.h"
#include "Diagnostics/NyanCadDevToolsWindow.h"
#include "MeshEditors/NyanFwSurfaceMeshEditor.h"
#include "MeshEditors/NyanFwTetMeshEditor.h"
#include "NyanCadDocument.h"
#include "NyanCadMainWindow.h"
#include "NyanCadWorkshopSetup.h"

#include "Matcha/Services/PluginHost.h"
#include "Matcha/Tree/Composition/Shell/Application.h"
#include "Matcha/Tree/Composition/Workbench/WorkbenchManager.h"
#include "Matcha/Tree/Composition/Workbench/WorkshopRegistry.h"
#include "Matcha/Services/DocumentManager.h"
#include "Matcha/Tree/Composition/Shell/Shell.h"
#include "Matcha/Tree/Composition/Shell/WindowNode.h"
#include "Matcha/Theming/NyanTheme.h"

#include <QApplication>
#include <QString>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

#include <print>

namespace nyancad {

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------

struct NyanCadApp::Impl {
#ifdef _WIN32
    HANDLE singleInstanceMutex = nullptr;
#endif
    std::unique_ptr<matcha::gui::NyanTheme> theme;
    std::unique_ptr<matcha::fw::Application> app;
    NyanCadMainWindow mainWindow;
    NyanCadDocumentHost docHost;
    NyanFwSurfaceMeshEditor surfaceMeshEditor;
    NyanFwTetMeshEditor tetMeshEditor;
    NyanFwSketchEditor sketchEditor;
    matcha::fw::PluginHost pluginHost;
    std::unique_ptr<NyanCadDevToolsWindow> devTools;
};

// ---------------------------------------------------------------------------
// Construction / Destruction
// ---------------------------------------------------------------------------

NyanCadApp::NyanCadApp()
    : _impl(std::make_unique<Impl>())
{
}

NyanCadApp::~NyanCadApp() = default;

// ---------------------------------------------------------------------------
// Single-Instance Guard
// ---------------------------------------------------------------------------

auto NyanCadApp::AcquireSingleInstanceLock() -> bool
{
#ifdef _WIN32
    _impl->singleInstanceMutex = CreateMutexW(nullptr, TRUE, L"NyanCad_SingleInstance");
    if (_impl->singleInstanceMutex == nullptr) {
        return false;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(_impl->singleInstanceMutex);
        _impl->singleInstanceMutex = nullptr;
        return false;
    }
    return true;
#else
    // Linux flock deferred -- always succeeds for now
    return true;
#endif
}

void NyanCadApp::ReleaseSingleInstanceLock()
{
#ifdef _WIN32
    if (_impl->singleInstanceMutex != nullptr) {
        ReleaseMutex(_impl->singleInstanceMutex);
        CloseHandle(_impl->singleInstanceMutex);
        _impl->singleInstanceMutex = nullptr;
    }
#endif
}

// ---------------------------------------------------------------------------
// Business Step (stub)
// ---------------------------------------------------------------------------

void NyanCadApp::BusinessStep()
{
    // Stub -- no real CAD engine. In a real app this would call:
    // myCadEngine.Step();
}

// ---------------------------------------------------------------------------
// Run
// ---------------------------------------------------------------------------

auto NyanCadApp::Run(int argc, char** argv) -> int
{
    // 1. Single-instance guard
    if (!AcquireSingleInstanceLock()) {
        std::println(stderr, "NyanCad: another instance is already running.");
        return 1;
    }

    // 2. QApplication (must exist before any QObject)
    QApplication qtApp(argc, argv);

    // 3. Theme service (set global accessor before any widget creation)
    _impl->theme = std::make_unique<matcha::gui::NyanTheme>(
        QString::fromUtf8(MATCHA_PALETTE_DIR));
    _impl->theme->SetTheme(matcha::gui::kThemeLight);
    // Register built-in icons from Resources/Icons directory
    _impl->theme->RegisterIconDirectory(
        matcha::fw::kMatchaIconPrefix,
        QString::fromUtf8(MATCHA_PALETTE_DIR) + QStringLiteral("/../Icons"));
    matcha::gui::SetThemeService(_impl->theme.get());

    // 4. Application + Shell + main WindowNode
    _impl->app = std::make_unique<matcha::fw::Application>();
    _impl->app->Initialize(argc, argv);

    // 控件重置阶段暂时停用文档、Workbench、插件和调试窗口装配。
    // 后续如果空壳链路稳定且这些旧装配不再需要，可以删除下方被注释的代码。
    //
    // // 5. 将文档宿主挂入命令树，通知从文档管理器向上传播到文档宿主
    // auto* docMgr = _impl->app->GetDocumentManagerImpl();
    // if (docMgr) { docMgr->SetParent(&_impl->docHost); }
    //
    // // 6. 注册 Workshop/Workbench 描述
    // auto* wsRegistry = _impl->app->GetWorkshopRegistry();
    // if (wsRegistry) {
    //     RegisterNyanCadWorkshops(*wsRegistry);
    // }
    //
    // // 7. 配置 NyanCad 主窗口，DocumentView 会订阅文档管理器通知
    // _impl->mainWindow.Setup(*_impl->app);
    //
    // // 8. 激活 Mesh Workshop，创建基础页签和默认 Workbench
    // auto* wbMgr = _impl->app->GetWorkbenchManager();
    // if (wbMgr) {
    //     wbMgr->ActivateWorkshop(matcha::fw::WorkshopId::From("mesh"));
    // }
    //
    // // 8. 从插件目录加载插件
    // auto& shell = _impl->app->GetShell();
    // auto pluginResult = _impl->pluginHost.LoadPluginsFromDirectory(
    //     MATCHA_PLUGIN_DIR, shell);
    // if (pluginResult.has_value()) {
    //     std::println("NyanCad: loaded {} plugins.", pluginResult.value().size());
    // }
    //
    // // 9. 创建初始演示文档，需在 UI 绑定后执行以便页签出现
    // if (docMgr) { _impl->docHost.CreateInitialDocuments(*docMgr); }
    //
    // // 9. 创建调试窗口，包括通知日志和 UI 检查器
    // _impl->devTools = std::make_unique<NyanCadDevToolsWindow>();
    // _impl->devTools->BuildWindow(_impl->app->MainWindow().Widget());
    // _impl->devTools->Bind(*_impl->app);
    // _impl->devTools->Show();

    // 10. Show main window
    _impl->app->MainWindow().Show();

    // 11. Business main loop (framework does NOT own event loop)
    while (!_impl->app->ShouldClose()) {
        _impl->app->Tick();
        BusinessStep();
    }

    // ===================================================================
    // Shutdown (see docs/05_Greenfield_Plan.md Appendix G)
    // ===================================================================

    // S1: Detach business observers
    //     Postcondition: all ScopedSubscriptions released.
    //     UiNode tree and Qt widgets are STILL ALIVE.
    // 控件重置阶段未执行 NyanCadMainWindow/DevTools 装配，相关清理暂时停用。
    // 后续如果对应装配代码删除，这里也应同步删除。
    // _impl->mainWindow.CloseFloatingWindows();
    // _impl->mainWindow.Teardown();
    // _impl->devTools.reset();

    // S2: Stop services
    //     Postcondition: no external events can arrive.
    _impl->app->MainWindow().Close();
    _impl->app->MainWindow().Hide();
    // 控件重置阶段未加载插件，相关清理暂时停用。
    // 后续如果插件启动路径恢复，这里需要同步恢复。
    // _impl->pluginHost.StopAll();

    // S3: Framework teardown
    //     Postcondition: UiNode tree gone, Qt widgets gone.
    _impl->app->Shutdown();

    // S4: Platform cleanup
    ReleaseSingleInstanceLock();
    QApplication::processEvents();

    return 0;
}

} // namespace nyancad
