#pragma once

#include <Matcha/Core/Macros.h>
#include <Matcha/Theming/ThemeAware.h>
#include <Matcha/Widgets/Menu/NyanMenuTypes.h>

#include <QElapsedTimer>
#include <QPointer>
#include <QWidget>

#include <cstdint>
#include <vector>

class QAction;
class QPainter;

namespace matcha::gui {

class NyanMenu;

class MATCHA_EXPORT NyanMenuBar : public QWidget, public ThemeAware {
    Q_OBJECT

public:
    explicit NyanMenuBar(QWidget* parent = nullptr);
    ~NyanMenuBar() override;

    NyanMenuBar(const NyanMenuBar&)            = delete;
    NyanMenuBar& operator=(const NyanMenuBar&) = delete;
    NyanMenuBar(NyanMenuBar&&)                 = delete;
    NyanMenuBar& operator=(NyanMenuBar&&)      = delete;

    void SetItems(const QList<NyanMenuItemData>& items);
    [[nodiscard]] auto Items() const -> QList<NyanMenuItemData>;
    void AddMenu(const NyanMenuItemData& menu);
    void AddMenu(const QString& key, const QString& text, const QList<NyanMenuItemData>& children);
    void Clear();

    // 兼容旧 API：返回顶层菜单对应的 NyanMenu，便于现有 MenuBarNode/MenuNode 继续填充菜单项。
    auto AddMenu(const QString& title) -> NyanMenu*;
    void RemoveMenu(NyanMenu* menu);
    [[nodiscard]] auto MenuAt(int index) const -> NyanMenu*;
    [[nodiscard]] auto MenuCount() const -> int;

    [[nodiscard]] auto sizeHint() const -> QSize override;
    [[nodiscard]] auto minimumSizeHint() const -> QSize override;

Q_SIGNALS:
    void MenuAboutToShow(NyanMenu* menu);
    void ItemsChanged();
    void ItemTriggered(const QString& key);
    void ActionTriggered(QAction* action);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void OnThemeChanged() override;

private:
    struct MenuEntry {
        NyanMenuItemData data;
        QPointer<NyanMenu> menu;
        QString rawTitle;
        QString displayText;
        QChar mnemonic;
    };

    [[nodiscard]] auto BarHeight() const -> int;
    [[nodiscard]] auto IndexAt(const QPoint& pos) const -> int;
    [[nodiscard]] auto FirstActivatableIndex() const -> int;
    [[nodiscard]] auto NextActivatableIndex(int from, int delta) const -> int;
    [[nodiscard]] auto IndexOfMenu(NyanMenu* menu) const -> int;
    [[nodiscard]] auto CanActivate(const MenuEntry& entry) const -> bool;

    void ParseTitle(MenuEntry& entry, const QString& title);
    void RebuildRects();
    void PaintEntry(QPainter& painter, int index);
    void OpenMenu(int index);
    void CloseActiveMenu();
    void ToggleMenu(int index);
    void NavigateMenu(int delta);
    void RegisterMenuMnemonics();
    void UnregisterMenuMnemonics();
    void SyncActionStates();
    void HookMenuSignals(NyanMenu* menu);

    QList<MenuEntry> _menus;
    QList<QRect> _itemRects;
    int _hoveredIndex = -1;
    int _pressedIndex = -1;
    int _activeIndex = -1;
    bool _menuOpen = false;
    bool _switchingMenu = false;
    bool _altPressedAlone = false;
    QElapsedTimer _dismissTimer;
    int _dismissedIndex = -1;
    std::vector<uint64_t> _mnemonicIds;
};

} // namespace matcha::gui
