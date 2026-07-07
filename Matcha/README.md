# Matcha

C++ GUI framework built on Qt 6, designed for CAD/CAE-class desktop applications.
Matcha provides a **UiNode abstraction layer** over Qt widgets, a **design-token theming engine**,
and a declarative **Workshop/Workbench architecture** inspired by the CATIA V5/3DEXPERIENCE platform.

---

## Key Features

| Category | Details |
|----------|---------|
| **Two-layer architecture** | UiNode tree (logic) + Nyan\* widget layer (rendering). Business code never touches Qt directly. |
| **Design-token theming** | Material 3 tonal palette, dynamic light/dark, WCAG 2.1 contrast audit, spring-physics animations. |
| **Workshop / Workbench** | Declarative descriptors, `WorkshopRegistry`, `WorkbenchManager` state machine, lazy command loading, push/pop stack, RAII `WorkbenchGuard`. |
| **Tabbed MDI documents** | `DocumentManager` + `DocumentArea` + `TabBarNode` with cross-window drag & drop. |
| **Multi-viewport** | `ViewportGroup` binary split tree, `IViewportRenderer` interface, viewport header bar overlay. |
| **50+ widgets** | CheckBox, ComboBox, Slider, RangeSlider, SpinBox, DataTable, PropertyGrid, ColorPicker, TreeWidget, Breadcrumb, Badge, Tag, ProgressRing, DateTimePicker, SearchBox, Paginator, CollapsibleSection, etc. |
| **Notification system** | Type-safe `Notification` classes, upward propagation through command tree, sync + async dispatch, 3-layer lifetime safety. |
| **Plugin system** | `IExpansionPlugin` + `PluginHost` for DLL-based addins; `IWorkshopContributor` for toolbar/command injection. |
| **C ABI** | `NyanCApi.h` stable FFI boundary for Python / Rust / C# bindings. |
| **Accessibility** | `A11yAudit`, contrast checking, focus management (`FocusManager`), keyboard navigation. |

---

## Architecture Overview

```
Layer 3  Application     NyanCad demo, business logic, plugins
Layer 2  Framework       WorkbenchManager, DocumentManager, Shell, Application
Layer 1  UiNode          WidgetNode, CommandNode, EventNode, Notification tree
Layer 0  Widget / Qt     Nyan* widgets (NyanPushButton, NyanActionBar, ...), QMainWindow
```

All public headers are in `Include/Matcha/`. The framework compiles to a single shared library (`Matcha.dll` / `libMatcha.so`).

**Design principle:** Business code programs against the UiNode API (Layer 1-2). The widget layer (Layer 0) is an implementation detail. This decoupling allows future rendering backends without breaking application code.

---

## Requirements

| Dependency | Version |
|------------|---------|
| C++ standard | C++23 |
| Compiler | Clang 21+ or MSVC 2022 |
| CMake | >= 3.28 |
| Qt | >= 6.7 (Widgets, Svg, Test modules) |
| Generator | Ninja (recommended) |

---

## Build

Matcha uses shared CMake presets plus a generated local preset file. The repository-level
`CMakePresets.json` intentionally does not contain machine-specific absolute paths. Local paths
for MSVC, Ninja, and Qt are stored in `config/local-dev.json`, then expanded into
`CMakeUserPresets.json` by `Scripts/generate_local_dev_config.ps1`.

### 1. Create the local configuration

From the project root:

```powershell
cd D:\CodeData\Matcha\Matcha
```

For a fresh checkout, copy the template first:

```powershell
Copy-Item -LiteralPath ".\config\local-dev.example.json" -Destination ".\config\local-dev.json"
```

Edit `config/local-dev.json` for the current machine:

```json
{
  "toolchain": {
    "msvcInitScript": "config/toolchain/windows/msvc-init.ps1",
    "vsInstallDir": "",
    "arch": "x64"
  },
  "tools": {
    "ninja": "D:/ProgramData/DevTools/toolchain/windows/ninja/ninja.exe"
  },
  "thirdParty": {
    "qtPrefix": "D:/ProgramData/DevTools/third_party/install/qt/6.10.2/msvc-debug",
    "extraPrefixes": []
  }
}
```

Important fields:

| Field | Meaning |
|-------|---------|
| `toolchain.msvcInitScript` | Project-local MSVC environment bootstrap script. Usually leave this as `config/toolchain/windows/msvc-init.ps1`. |
| `toolchain.vsInstallDir` | Optional Visual Studio installation directory. Leave empty to auto-detect with `vswhere` / common paths. |
| `tools.ninja` | Absolute path to `ninja.exe`. |
| `thirdParty.qtPrefix` | Qt installation prefix. This directory should contain `bin/windeployqt.exe` and `lib/cmake/Qt6`. |
| `thirdParty.extraPrefixes` | Optional additional CMake package prefixes. |

`config/local-dev.json` is ignored by Git and should not be committed.

### 2. Generate local CMake presets

Run:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\Scripts\generate_local_dev_config.ps1"
```

The script:

- runs `config/toolchain/windows/msvc-init.ps1`;
- imports MSVC environment variables such as `PATH`, `INCLUDE`, `LIB`, and `LIBPATH`;
- writes `CMakeUserPresets.json`;
- writes `.vscode/settings.json` for CMake Tools and C/C++ IntelliSense;
- writes `.vscode/tasks.json` and `.vscode/launch.json` for building and debugging `NyanCad.exe`;
- writes `.vscode/c_cpp_properties.json` as a fallback C/C++ extension configuration;
- sets `CMAKE_MAKE_PROGRAM` to the configured Ninja path;
- sets `CMAKE_PREFIX_PATH` to the configured Qt prefix and any `extraPrefixes`.

Generated files:

```text
CMakeUserPresets.json
.vscode/settings.json
.vscode/tasks.json
.vscode/launch.json
.vscode/c_cpp_properties.json
```

Both are local machine files and are ignored by Git.

### 3. Configure

```powershell
cmake --preset local-windows-msvc-debug
```

This configures the project into:

```text
build/local-windows-msvc-debug
```

The preset `local-windows-msvc-debug` is generated in `CMakeUserPresets.json` and inherits the
shared `windows-msvc-debug` preset from `CMakePresets.json`.

### 4. Build

```powershell
cmake --build --preset local-windows-msvc-debug
```

By default, this builds the framework, the `NyanCad` demo executable, and the NyanCad plugin DLLs.
Test targets are not built by default. To include tests, configure with:

```powershell
cmake --preset local-windows-msvc-debug -DMATCHA_BUILD_TESTS=ON
cmake --build --preset local-windows-msvc-debug
```

Warnings are not treated as errors by default for local development. To enable strict warning
handling:

```powershell
cmake --preset local-windows-msvc-debug -DMATCHA_WARNINGS_AS_ERRORS=ON
```

### 5. Qt runtime deployment

After each executable target is built, CMake runs the Qt deployment script generated by
`qt_generate_deploy_app_script()`. For `NyanCad`, the post-build step runs Qt's `windeployqt.exe`
and deploys the required Qt runtime files next to the executable.

Expected output layout:

```text
build/local-windows-msvc-debug/
  NyanCad.exe
  Matcha.dll
  Qt6Cored.dll
  Qt6Guid.dll
  Qt6Networkd.dll
  Qt6Svgd.dll
  Qt6Widgetsd.dll
  qt.conf
  plugins/
    platforms/
      qwindowsd.dll
```

The Qt DLLs are placed in the same directory as `NyanCad.exe`; Qt plugins remain under the
standard `plugins/` subdirectories.

### 6. Run

```powershell
.\build\local-windows-msvc-debug\NyanCad.exe
```

Because Qt deployment runs as part of the build, running `NyanCad.exe` does not require manually
editing `PATH` for Qt DLLs.

### 7. Use VS Code

Install these VS Code extensions:

- CMake Tools (`ms-vscode.cmake-tools`)
- C/C++ (`ms-vscode.cpptools`)

After running `Scripts/generate_local_dev_config.ps1`, open the `Matcha` folder in VS Code. The
generated `.vscode/settings.json` enables CMake presets and selects:

```text
Configure Preset: local-windows-msvc-debug
Build Preset:     local-windows-msvc-debug
```

The generated `.vscode/tasks.json` provides:

| Task | Behavior |
|------|----------|
| `CMake: configure` | Runs `cmake --preset local-windows-msvc-debug`. |
| `CMake: build NyanCad` | Runs `cmake --build --preset local-windows-msvc-debug --target NyanCad`. |

The generated `.vscode/launch.json` provides this debug configuration:

```text
Debug NyanCad
```

Select **Run and Debug -> Debug NyanCad** in VS Code. The launch configuration runs the
`CMake: build NyanCad` pre-launch task first, then starts:

```text
${workspaceFolder}/build/local-windows-msvc-debug/NyanCad.exe
```

with the working directory set to:

```text
${workspaceFolder}/build/local-windows-msvc-debug
```

This uses Visual Studio's native Windows debugger through VS Code's `cppvsdbg` debug adapter.
Breakpoints in project source files should bind once the target is built with debug information.

### 8. Code completion and navigation

Code completion, go-to-definition, and symbol navigation are configured through two generated
settings:

```json
{
  "C_Cpp.default.configurationProvider": "ms-vscode.cmake-tools",
  "C_Cpp.default.compileCommands": "${workspaceFolder}/build/local-windows-msvc-debug/compile_commands.json"
}
```

Preferred path: C/C++ asks CMake Tools for the active target configuration, include directories,
defines, compiler path, and language standard.

Fallback path: `.vscode/c_cpp_properties.json` points directly to:

```text
build/local-windows-msvc-debug/compile_commands.json
```

If completion or code navigation looks stale:

1. Run `Scripts/generate_local_dev_config.ps1` again after editing `config/local-dev.json`.
2. Run `cmake --preset local-windows-msvc-debug` so `compile_commands.json` exists.
3. In VS Code, run **C/C++: Reset IntelliSense Database** from the command palette.
4. Check that CMake Tools shows `local-windows-msvc-debug` as the active configure preset.

### Troubleshooting

| Error | Check |
|-------|-------|
| `Local development config was not found` | Copy `config/local-dev.example.json` to `config/local-dev.json`. |
| `Ninja executable was not found` | Fix `tools.ninja` in `config/local-dev.json`. |
| `Qt prefix was not found` | Fix `thirdParty.qtPrefix`; it must be the Qt install prefix, not `bin` or `lib/cmake/Qt6`. |
| `Visual Studio with C++ tools was not found` | Install Visual Studio Build Tools, or set `toolchain.vsInstallDir`. |
| CMake cannot find Qt | Regenerate `CMakeUserPresets.json` after editing `config/local-dev.json`. |

---

## Test

```bash
ctest --preset debug
```

The test suite uses [doctest](https://github.com/doctest/doctest) and Qt Test:

- **Unit tests** (`Tests/Unit/`) -- pure logic, no `QApplication` required.
- **Integration tests** (`Tests/Integration/`) -- headless `QApplication` via `WidgetTestFixture`, offscreen rendering.

Running a specific test binary directly:

```bash
# Unit tests only
./build/debug/MatchaUnitTests.exe --no-color

# Integration tests only (requires Qt plugins in PATH)
./build/debug/MatchaIntegrationTests.exe --no-color
```

---

## Run Demo

```bash
# After building, run the NyanCad demo (requires Qt DLLs in PATH)
./build/debug/NyanCad.exe
```

NyanCad is a minimal CAD application demonstrating Workshop/Workbench switching, multi-document tabs, viewport splitting, and the full widget catalog.

---

## Quick Start

### Creating UiNodes

Business code works exclusively with UiNode wrappers -- never with Qt widgets directly:

```cpp
#include <Matcha/UiNodes/Controls/LabelNode.h>
#include <Matcha/UiNodes/Controls/PushButtonNode.h>
#include <Matcha/UiNodes/Controls/LineEditNode.h>

// Create typed UiNode wrappers
auto label = std::make_unique<matcha::fw::LabelNode>("my-label");
label->SetText("Hello Matcha");

auto button = std::make_unique<matcha::fw::PushButtonNode>("btn-ok");
button->SetText("OK");
button->SetIcon(matcha::fw::icons::Check);

auto input = std::make_unique<matcha::fw::LineEditNode>("input-name");
input->SetPlaceholder("Enter name...");
input->SetEnabled(true);

// Add to a container
container->AddNode(std::move(label));
container->AddNode(std::move(button));
container->AddNode(std::move(input));
```

### Workshop / Workbench Registration

Define workshops and workbenches declaratively, then let `WorkbenchManager` materialize the UI:

```cpp
#include <Matcha/UiNodes/Workbench/WorkshopRegistry.h>
#include <Matcha/UiNodes/Workbench/WorkbenchTypes.h>

using namespace matcha::fw;

// 1. Define commands
CommandHeaderDescriptor saveCmd;
saveCmd.id      = CmdHeaderId::From("cmd.save");
saveCmd.label   = "Save";
saveCmd.iconId  = "asset://matcha/icons/save";
saveCmd.tooltip = "Save Document";

// 2. Build tab blueprint
TabBlueprint fileTab;
fileTab.tabId = "file";
fileTab.label = "File";
ToolbarBlueprint tb;
tb.toolbarId = "file_ops";
tb.label     = "File Operations";
tb.commands  = { CmdHeaderId::From("cmd.save") };
fileTab.toolbars.push_back(std::move(tb));

// 3. Assemble workshop descriptor
WorkshopDescriptor ws;
ws.id    = WorkshopId::From("mesh");
ws.label = "Mesh";
ws.commands.push_back(std::move(saveCmd));
ws.baseTabs.push_back(std::move(fileTab));
ws.defaultWorkbenchId = WorkbenchId::From("surface_mesh");
ws.workbenchIds = { WorkbenchId::From("surface_mesh") };

// 4. Register and activate
registry.RegisterWorkshop(std::move(ws));
// ... register workbenches ...
wbMgr->ActivateWorkshop(WorkshopId::From("mesh"));
```

### Subscribing to Notifications

Notifications propagate upward through the command tree:

```cpp
#include <Matcha/UiNodes/Core/EventNode.h>

// Subscribe to a specific notification type from a sender
auto sub = subscriber->Subscribe<matcha::fw::WorkbenchActivated>(
    sender,
    [](matcha::fw::WorkbenchActivated& notif) {
        // Handle workbench activation
    });

// ScopedSubscription auto-unsubscribes on destruction
// Store it in a member variable for lifetime management
```

---

## Widget Catalog

### Input Controls

| UiNode | Widget | Description |
|--------|--------|-------------|
| `LineEditNode` | `NyanLineEdit` | Single-line text input with validation |
| `PlainTextEditNode` | -- | Multi-line plain text editor |
| `SpinBoxNode` | `NyanSpinBox` | Integer spinner |
| `DoubleSpinBoxNode` | `NyanDoubleSpinBox` | Floating-point spinner |
| `SliderNode` | `NyanSlider` | Single-value slider |
| `RangeSliderNode` | `NyanRangeSlider` | Dual-handle range slider |
| `ComboBoxNode` | `NyanComboBox` | Dropdown selection |
| `CheckBoxNode` | `NyanCheckBox` | Boolean checkbox |
| `RadioButtonNode` | `NyanRadioButton` | Exclusive radio button |
| `ToggleSwitchNode` | `NyanToggleSwitch` | On/off toggle |
| `SearchBoxNode` | `NyanSearchBox` | Search input with filtering |
| `ColorPickerNode` | `NyanColorPicker` | Color selection dialog |
| `DateTimePickerNode` | `NyanDateTimePicker` | Date/time input |

### Display Controls

| UiNode | Widget | Description |
|--------|--------|-------------|
| `LabelNode` | `NyanLabel` | Static text / rich text display |
| `BadgeNode` | `NyanBadge` | Numeric or dot badge |
| `ProgressBarNode` | `NyanProgressBar` | Linear progress indicator |
| `ProgressRingNode` | `NyanProgressRing` | Circular progress indicator |
| `ColorSwatchNode` | `NyanColorSwatch` | Color preview tile |
| `LineNode` | `NyanLine` | 1px themed separator |

### Action Controls

| UiNode | Widget | Description |
|--------|--------|-------------|
| `PushButtonNode` | `NyanPushButton` | Standard button (primary / secondary / ghost) |
| `ToolButtonNode` | `NyanToolButton` | Compact toolbar button |

### Data Controls

| UiNode | Widget | Description |
|--------|--------|-------------|
| `DataTableNode` | `NyanDataTable` | Sortable/filterable data grid |
| `ListWidgetNode` | `NyanListWidget` | Vertical list |
| `TreeWidgetNode` | `NyanStructureTree` | Hierarchical tree with pin button |
| `PropertyGridNode` | `NyanPropertyGrid` | Key-value property editor |
| `PaginatorNode` | `NyanPaginator` | Page navigation |

### Layout & Navigation

| UiNode | Widget | Description |
|--------|--------|-------------|
| `CollapsibleSectionNode` | `NyanCollapsibleSection` | Expandable section with header |
| -- | `NyanBreadcrumb` | Path-style navigation |
| -- | `NyanTag` | Removable tag / chip |
| -- | `NyanGroupBox` | Titled container box |

### Feedback

| Widget | Description |
|--------|-------------|
| `NyanRichTooltip` | Two-tier rich tooltip (brief + detailed) with icon, shortcut, preview |
| `NyanMessage` | Inline message bar (info / success / warning / error) |
| `NyanNotification` | Toast notification with auto-dismiss |
| `NyanPopConfirm` | Confirmation popover |
| `NyanInputDialog` | Modal input dialog |

---

## Design Token Theming

Matcha uses a Material 3 inspired design token system:

- **Color tokens**: `Primary`, `OnPrimary`, `Surface`, `OnSurface`, `Error`, etc.
- **Tonal palette**: Auto-generated from a seed color via `TonalPaletteGenerator`.
- **Typography tokens**: `DisplayLarge` .. `LabelSmall` (15 levels).
- **Spacing tokens**: `Px2`, `Px4`, `Px8`, `Px12`, `Px16`, `Px24`, `Px32`.
- **Elevation tokens**: `Level0` .. `Level5` with shadow parameters.
- **Animation tokens**: Spring-physics based (`SpringAnimation`), state transitions (`StateTransition`).
- **Icon system**: Open URI-based `IconId` (`asset://matcha/icons/save`), SVG colorization, runtime directory registration.

Theme changes propagate automatically to all `ThemeAware` widgets via the `OnThemeChanged()` virtual.

---

## Notification System

Matcha uses a type-safe notification architecture for decoupled communication:

```
Sender --> Parent --> Grandparent --> ... --> Shell (root)
```

- **Direction**: Notifications propagate **upward** through the `CommandNode` tree.
- **Dispatch modes**: Synchronous (`SendNotification`) and asynchronous (`SendNotificationQueued`).
- **Lifetime safety**: 3-layer defense (subscriber token, publisher token, generation counter).
- **Subscription**: `ScopedSubscription` RAII for automatic cleanup.

Notification categories:

| Category | Examples |
|----------|----------|
| Widget | `VisibilityChanged`, `EnabledChanged`, `FocusChanged` |
| Drag & Drop | `DragEntered`, `DragMoved`, `DragLeft`, `Dropped` |
| Document | `DocumentCreated`, `DocumentSwitched`, `DocumentClosing`, `DocumentClosed` |
| Tab | `TabDroppedIn`, `TabReordered`, `TabPageDraggedOut` |
| Viewport | `ActiveVpChanged`, `VpCreated`, `VpRemoved`, `VpSplitRatioChanged` |
| Workbench | `WorkshopActivated`, `WorkbenchActivated`, `CommandInvoked` |

---

## Project Structure

```
Matcha/
  Include/Matcha/
    Foundation/        Core types: StrongId, StringId, observer_ptr, WidgetEnums
    UiNodes/
      Core/            WidgetNode, CommandNode, EventNode, FocusManager, MetaClass
      Shell/           Shell, WindowNode, MainTitleBarNode, StatusBarNode, WorkspaceFrame
      ActionBar/       ActionBarNode, ActionTabNode, ActionToolbarNode, ActionButtonNode
      Controls/        30 typed UiNode wrappers (LabelNode .. TreeWidgetNode)
      Document/        DocumentArea, DocumentPage, TabBarNode, Viewport, ViewportGroup
      Menu/            MenuBarNode, MenuNode, DialogNode
      Workbench/       WorkbenchManager, WorkshopRegistry, WorkbenchTypes
    Widgets/
      Core/            NyanTheme, AnimationService, SvgIconProvider, UpdateGuard
      Controls/        41 themed Qt widgets (NyanPushButton .. NyanStructureTree)
      ActionBar/       NyanActionBar, NyanPanel, ActionBarFloatingFrame
      Shell/           NyanMainTitleBar, ViewportFrame, NyanSplitter, NyanScrollArea
      Menu/            NyanMenu, NyanDialog, NyanPopConfirm, NyanInputDialog
    Services/          DocumentManager, PluginHost, IViewportRenderer
    CApi/              C ABI header (NyanCApi.h)
  Source/              Implementation (.cpp)
  Tests/
    Unit/              doctest unit tests (logic, no Qt event loop)
    Integration/       doctest + QApplication integration tests
    TestUtils/         WidgetTestFixture, test helpers
  Demos/NyanCad/       Demo CAD application
  Resources/           Icons, palettes, stylesheets
  docs/                Design System Specification
```

---

## Documentation

The full design specification is at `docs/Matcha_Design_System_Specification.md` (~9000 lines), covering:

- Design tokens (color, typography, spacing, elevation, animation)
- Widget specifications (28 chapters, each with states, anatomy, theming, a11y)
- Workshop/Workbench architecture (Appendix C)
- Business-layer ABI boundary (Appendix D)
- Application architecture glossary (Appendix E)

---

## Coding Standards

| Rule | Detail |
|------|--------|
| Language standard | C++23, no modules (header-only public API) |
| Naming: classes | `PascalCase` (`WidgetNode`, `WorkshopRegistry`) |
| Naming: methods | `PascalCase` (`SetEnabled`, `ActivateWorkshop`) |
| Naming: members | `_camelCase` prefix (`_activeWorkshopId`) |
| Naming: constants | `kPascalCase` (`kDefaultTier1`, `kMaxWidth`) |
| Documentation | Doxygen-style (`@brief`, `@param`, `@return`) |
| Error handling | Return `bool` or `ErrorCode`; no exceptions across DLL boundaries |
| Thread safety | All UI mutations assert Qt main thread |
| Memory | `std::unique_ptr` ownership, `observer_ptr` for non-owning refs |

---

## License

Proprietary.
