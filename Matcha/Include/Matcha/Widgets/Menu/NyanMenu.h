#pragma once

#include <Matcha/Core/Macros.h>
#include <Matcha/Theming/ThemeAware.h>
#include <Matcha/Widgets/Menu/NyanMenuTypes.h>

#include <QPointer>
#include <QTimer>
#include <QWidget>

class QParallelAnimationGroup;

namespace matcha::gui {

class NyanMenuCheckItem;
class NyanMenuItem;
class NyanMenuSeparator;

class MATCHA_EXPORT NyanMenu : public QWidget, public ThemeAware {
    Q_OBJECT

public:
    explicit NyanMenu(QWidget* parent = nullptr);
    ~NyanMenu() override;

    NyanMenu(const NyanMenu&)            = delete;
    NyanMenu& operator=(const NyanMenu&) = delete;
    NyanMenu(NyanMenu&&)                 = delete;
    NyanMenu& operator=(NyanMenu&&)      = delete;

    void SetItems(const QList<NyanMenuItemData>& items);
    [[nodiscard]] auto Items() const -> QList<NyanMenuItemData>;
    void AddItem(const NyanMenuItemData& item);
    void Clear();
    [[nodiscard]] auto ItemCount() const -> int;

    // 兼容旧 API：返回的 NyanMenuItem 仅作为状态和信号绑定代理，不再参与菜单项绘制。
    auto AddItem(const QString& text, const QIcon& icon = {}) -> NyanMenuItem*;
    auto AddSeparator() -> NyanMenuSeparator*;
    auto AddCheckItem(const QString& text, bool checked = false) -> NyanMenuCheckItem*;
    auto AddSubmenu(const QString& text, const QIcon& icon = {}) -> NyanMenu*;
    void AddWidget(QWidget* widget);

    void PopupAt(const QPoint& globalPos, NyanPopupPlacement placement = NyanPopupPlacement::Context);
    void PopupBelow(const QPoint& globalPos, int anchorWidth = 0);
    void PopupBeside(const QPoint& globalPos, int anchorHeight = 0);
    void CloseAll();

    // 兼容旧 API。
    void Popup(const QPoint& globalPos);
    void Close();
    [[nodiscard]] auto IsOpen() const -> bool;
    [[nodiscard]] auto IsSubmenu() const -> bool;
    [[nodiscard]] auto ParentMenu() const -> NyanMenu*;
    [[nodiscard]] auto ActiveSubmenu() const -> NyanMenu*;
    void HandleExternalMouseMove(const QPoint& globalPos);

    [[nodiscard]] auto sizeHint() const -> QSize override;
    [[nodiscard]] auto minimumSizeHint() const -> QSize override;

Q_SIGNALS:
    void AboutToShow();
    void AboutToHide();
    void ItemsChanged();
    void ItemTriggered(NyanMenuItem* item);
    void ItemKeyTriggered(const QString& key);
    void ActionTriggered(QAction* action);
    void SubmenuRequested(const QString& key, const QRect& itemRect);
    void CloseRequested();
    void MouseExitedToward(QPoint globalPos);

protected:
    auto event(QEvent* event) -> bool override;
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void OnThemeChanged() override;

private:
    struct ItemRuntime {
        NyanMenuItemData data;
        QPointer<NyanMenuItem> itemProxy;
        QPointer<NyanMenuCheckItem> checkProxy;
        QPointer<NyanMenuSeparator> separatorProxy;
        QPointer<NyanMenu> submenuProxy;
        QPointer<QWidget> widgetProxy;
    };

    [[nodiscard]] auto PopupSize() const -> QSize;
    [[nodiscard]] auto ContentRect() const -> QRect;
    [[nodiscard]] auto AvailableGeometry(const QPoint& pos) const -> QRect;
    [[nodiscard]] auto AdjustedGeometry(const QPoint& globalPos, NyanPopupPlacement placement) const -> QRect;
    [[nodiscard]] auto CollapsedGeometry(const QRect& target,
                                         const QPoint& globalPos,
                                         NyanPopupPlacement placement) const -> QRect;
    [[nodiscard]] auto IndexAt(const QPoint& pos) const -> int;
    [[nodiscard]] auto FirstActivatableIndex() const -> int;
    [[nodiscard]] auto NextActivatableIndex(int from, int delta) const -> int;
    [[nodiscard]] auto CanActivate(const NyanMenuItemData& item) const -> bool;
    [[nodiscard]] auto ItemHeight(const NyanMenuItemData& item) const -> int;

    void StartPopupAnimation(const QRect& target, const QPoint& globalPos, NyanPopupPlacement placement);
    void StopPopupAnimation();
    void RebuildRects();
    void PaintPopupFrame(QPainter& painter);
    void PaintItem(QPainter& painter, const NyanMenuItemData& item, const QRect& rect, int index);
    void TriggerItem(int index);
    void OpenSubmenu(int index);
    void CloseChildPopup();
    void MoveActive(int delta);
    void SyncActionStates();
    void AppendRuntime(ItemRuntime runtime);
    void ConnectAction(QAction* action);
    void RequestSubmenuOpen(int index);

    static constexpr int kPopupOffset = 2;
    static constexpr int kPopupShadowMargin = 10;
    static constexpr int kPopupMinWidth = 160;
    static constexpr int kPopupMaxHeight = 360;
    static constexpr int kMotionDurationMs = 120;
    static constexpr int kSubmenuDelayMs = 120;

    QList<ItemRuntime> _items;
    QList<QRect> _itemRects;
    QPointer<NyanMenu> _childPopup;
    QPointer<QParallelAnimationGroup> _popupAnimation;
    QTimer* _submenuTimer = nullptr;
    int _hoveredIndex = -1;
    int _pressedIndex = -1;
    int _activeIndex = -1;
    int _scrollOffset = 0;
    int _pendingSubmenuIndex = -1;
    bool _isSubmenu = false;
    bool _explicitClose = false;
};

} // namespace matcha::gui
