#include <Matcha/Widgets/Menu/NyanMenu.h>

#include <Matcha/Interaction/Focus/MnemonicState.h>
#include <Matcha/Widgets/Menu/NyanMenuCheckItem.h>
#include <Matcha/Widgets/Menu/NyanMenuItem.h>
#include <Matcha/Widgets/Menu/NyanMenuSeparator.h>

#include <QAbstractAnimation>
#include <QAction>
#include <QApplication>
#include <QFocusEvent>
#include <QGuiApplication>
#include <QHideEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QScreen>
#include <QWheelEvent>

#include <algorithm>

namespace matcha::gui {

namespace {

[[nodiscard]] auto displayTextOf(const QString& rawText) -> QString
{
    return MnemonicState::Parse(rawText).displayText;
}

} // namespace

NyanMenu::NyanMenu(QWidget* parent)
    : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint)
    , ThemeAware(WidgetKind::Menu)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);

    _submenuTimer = new QTimer(this);
    _submenuTimer->setSingleShot(true);
    connect(_submenuTimer, &QTimer::timeout, this, [this]() {
        if (_pendingSubmenuIndex >= 0) {
            OpenSubmenu(_pendingSubmenuIndex);
        }
    });
}

NyanMenu::~NyanMenu()
{
    StopPopupAnimation();
}

void NyanMenu::SetItems(const QList<NyanMenuItemData>& items)
{
    Clear();
    for (const auto& item : items) {
        AddItem(item);
    }
}

auto NyanMenu::Items() const -> QList<NyanMenuItemData>
{
    QList<NyanMenuItemData> result;
    result.reserve(_items.size());
    for (const auto& item : _items) {
        result.append(item.data);
    }
    return result;
}

void NyanMenu::AddItem(const NyanMenuItemData& item)
{
    ItemRuntime runtime;
    runtime.data = item;
    AppendRuntime(std::move(runtime));
}

auto NyanMenu::AddItem(const QString& text, const QIcon& icon) -> NyanMenuItem*
{
    auto* proxy = new NyanMenuItem(this);
    proxy->SetText(text);
    proxy->SetIcon(icon);
    proxy->hide();

    ItemRuntime runtime;
    runtime.data = NyanMenuItemData::ActionItem(text, text);
    runtime.data.icon = icon;
    runtime.itemProxy = proxy;
    AppendRuntime(std::move(runtime));
    return proxy;
}

auto NyanMenu::AddSeparator() -> NyanMenuSeparator*
{
    auto* proxy = new NyanMenuSeparator(this);
    proxy->hide();

    ItemRuntime runtime;
    runtime.data = NyanMenuItemData::Divider();
    runtime.separatorProxy = proxy;
    AppendRuntime(std::move(runtime));
    return proxy;
}

auto NyanMenu::AddCheckItem(const QString& text, bool checked) -> NyanMenuCheckItem*
{
    auto* proxy = new NyanMenuCheckItem(this);
    proxy->SetText(text);
    proxy->SetChecked(checked);
    proxy->hide();

    ItemRuntime runtime;
    runtime.data = NyanMenuItemData::ActionItem(text, text);
    runtime.data.checkable = true;
    runtime.data.checked = checked;
    runtime.itemProxy = proxy;
    runtime.checkProxy = proxy;
    AppendRuntime(std::move(runtime));
    return proxy;
}

auto NyanMenu::AddSubmenu(const QString& text, const QIcon& icon) -> NyanMenu*
{
    auto* proxy = new NyanMenuItem(this);
    proxy->SetText(text);
    proxy->SetIcon(icon);
    proxy->SetSubmenuIndicator(true);
    proxy->hide();

    auto* submenu = new NyanMenu(this);
    submenu->_isSubmenu = true;

    ItemRuntime runtime;
    runtime.data = NyanMenuItemData::SubMenu(text, text, {});
    runtime.data.icon = icon;
    runtime.itemProxy = proxy;
    runtime.submenuProxy = submenu;
    AppendRuntime(std::move(runtime));
    return submenu;
}

void NyanMenu::AddWidget(QWidget* widget)
{
    if (widget == nullptr) {
        return;
    }

    widget->setParent(this);
    widget->hide();

    ItemRuntime runtime;
    runtime.data = NyanMenuItemData::Group(widget->objectName());
    runtime.widgetProxy = widget;
    AppendRuntime(std::move(runtime));
}

void NyanMenu::AppendRuntime(ItemRuntime runtime)
{
    if (runtime.data.key.isEmpty()) {
        runtime.data.key = runtime.data.text;
    }
    if (runtime.itemProxy != nullptr) {
        runtime.itemProxy->setEnabled(runtime.data.enabled);
    }
    if (runtime.data.action != nullptr) {
        ConnectAction(runtime.data.action);
    }

    _items.append(std::move(runtime));
    RebuildRects();
    updateGeometry();
    update();
    Q_EMIT ItemsChanged();
}

void NyanMenu::Clear()
{
    CloseChildPopup();
    StopPopupAnimation();

    for (const auto& item : _items) {
        delete item.itemProxy;
        if (item.separatorProxy != nullptr) {
            delete item.separatorProxy;
        }
        if (item.submenuProxy != nullptr) {
            delete item.submenuProxy;
        }
        if (item.widgetProxy != nullptr) {
            delete item.widgetProxy;
        }
    }

    _items.clear();
    _itemRects.clear();
    _hoveredIndex = -1;
    _pressedIndex = -1;
    _activeIndex = -1;
    _scrollOffset = 0;
    _pendingSubmenuIndex = -1;
    updateGeometry();
    update();
    Q_EMIT ItemsChanged();
}

auto NyanMenu::ItemCount() const -> int
{
    return _items.size();
}

void NyanMenu::PopupAt(const QPoint& globalPos, NyanPopupPlacement placement)
{
    Q_EMIT AboutToShow();

    SyncActionStates();
    const QRect target = AdjustedGeometry(globalPos, placement);
    StartPopupAnimation(target, globalPos, placement);
    show();
    raise();
    setFocus();
}

void NyanMenu::PopupBelow(const QPoint& globalPos, int /*anchorWidth*/)
{
    PopupAt(globalPos, NyanPopupPlacement::Below);
}

void NyanMenu::PopupBeside(const QPoint& globalPos, int /*anchorHeight*/)
{
    PopupAt(globalPos, NyanPopupPlacement::Right);
}

void NyanMenu::Popup(const QPoint& globalPos)
{
    PopupBelow(globalPos);
}

void NyanMenu::CloseAll()
{
    _explicitClose = true;
    CloseChildPopup();
    StopPopupAnimation();
    setWindowOpacity(1.0);
    Q_EMIT AboutToHide();
    hide();
    _explicitClose = false;
}

void NyanMenu::Close()
{
    CloseAll();
}

auto NyanMenu::IsOpen() const -> bool
{
    return isVisible();
}

auto NyanMenu::IsSubmenu() const -> bool
{
    return _isSubmenu;
}

auto NyanMenu::ParentMenu() const -> NyanMenu*
{
    return _isSubmenu ? qobject_cast<NyanMenu*>(parentWidget()) : nullptr;
}

auto NyanMenu::ActiveSubmenu() const -> NyanMenu*
{
    return _childPopup;
}

void NyanMenu::HandleExternalMouseMove(const QPoint& globalPos)
{
    const QPoint localPos = mapFromGlobal(globalPos);
    if (!rect().contains(localPos)) {
        return;
    }

    const int index = IndexAt(localPos);
    if (index >= 0) {
        _hoveredIndex = index;
        _activeIndex = index;
        RequestSubmenuOpen(index);
        update();
    }
}

auto NyanMenu::sizeHint() const -> QSize
{
    return PopupSize();
}

auto NyanMenu::minimumSizeHint() const -> QSize
{
    return {kPopupMinWidth, Theme().DimensionPx("controlHeightMD").value_or(32)};
}

auto NyanMenu::event(QEvent* event) -> bool
{
    if (event->type() == QEvent::WindowDeactivate && !_isSubmenu) {
        CloseAll();
        return true;
    }
    return QWidget::event(event);
}

void NyanMenu::paintEvent(QPaintEvent* /*event*/)
{
    SyncActionStates();
    RebuildRects();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QFont resolvedFont = font();
    if (const auto spec = Theme().Font("fontSM")) {
        resolvedFont.setPointSize(spec->sizeInPt);
    } else {
        resolvedFont.setPixelSize(12);
    }
    painter.setFont(resolvedFont);

    PaintPopupFrame(painter);
    painter.save();
    painter.setClipRect(ContentRect());
    for (int i = 0; i < _items.size() && i < _itemRects.size(); ++i) {
        if (!_items[i].data.visible) {
            continue;
        }
        PaintItem(painter, _items[i].data, _itemRects[i], i);
    }
    painter.restore();
}

void NyanMenu::mouseMoveEvent(QMouseEvent* event)
{
    const QPoint localPos = event->pos();
    if (!rect().contains(localPos)) {
        Q_EMIT MouseExitedToward(event->globalPosition().toPoint());
        QWidget::mouseMoveEvent(event);
        return;
    }

    const int index = IndexAt(localPos);
    if (index != _hoveredIndex) {
        _hoveredIndex = index;
        _activeIndex = index;
        update();
    }
    RequestSubmenuOpen(index);
    QWidget::mouseMoveEvent(event);
}

void NyanMenu::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        const int index = IndexAt(event->pos());
        if (index >= 0 && CanActivate(_items[index].data)) {
            _pressedIndex = index;
            update();
        }
    }
    QWidget::mousePressEvent(event);
}

void NyanMenu::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        const int index = IndexAt(event->pos());
        const int pressed = _pressedIndex;
        _pressedIndex = -1;
        if (index >= 0 && index == pressed) {
            TriggerItem(index);
        }
        update();
    }
    QWidget::mouseReleaseEvent(event);
}

void NyanMenu::leaveEvent(QEvent* event)
{
    _hoveredIndex = -1;
    update();
    QWidget::leaveEvent(event);
}

void NyanMenu::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_Down:
        MoveActive(1);
        event->accept();
        return;
    case Qt::Key_Up:
        MoveActive(-1);
        event->accept();
        return;
    case Qt::Key_Home:
        _activeIndex = FirstActivatableIndex();
        _hoveredIndex = _activeIndex;
        update();
        event->accept();
        return;
    case Qt::Key_End:
        _activeIndex = NextActivatableIndex(0, -1);
        _hoveredIndex = _activeIndex;
        update();
        event->accept();
        return;
    case Qt::Key_Right:
        if (_activeIndex >= 0 && _items[_activeIndex].data.type == NyanMenuItemType::SubMenu) {
            OpenSubmenu(_activeIndex);
            event->accept();
            return;
        }
        break;
    case Qt::Key_Left:
        if (_isSubmenu) {
            CloseAll();
            if (auto* parent = ParentMenu()) {
                parent->setFocus();
            }
            event->accept();
            return;
        }
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Space:
        if (_activeIndex >= 0) {
            TriggerItem(_activeIndex);
            event->accept();
            return;
        }
        break;
    case Qt::Key_Escape:
        Q_EMIT CloseRequested();
        CloseAll();
        event->accept();
        return;
    default:
        break;
    }
    QWidget::keyPressEvent(event);
}

void NyanMenu::wheelEvent(QWheelEvent* event)
{
    const int contentHeight = std::max(0, PopupSize().height() - kPopupShadowMargin * 2);
    const int visibleHeight = ContentRect().height();
    const int maxOffset = std::max(0, contentHeight - visibleHeight);
    _scrollOffset = std::clamp(_scrollOffset - event->angleDelta().y() / 3, 0, maxOffset);
    RebuildRects();
    update();
    event->accept();
}

void NyanMenu::hideEvent(QHideEvent* event)
{
    CloseChildPopup();
    StopPopupAnimation();
    _hoveredIndex = -1;
    _pressedIndex = -1;
    _activeIndex = -1;
    setWindowOpacity(1.0);
    if (!_explicitClose) {
        Q_EMIT AboutToHide();
    }
    QWidget::hideEvent(event);
}

void NyanMenu::focusOutEvent(QFocusEvent* event)
{
    if (_childPopup != nullptr && _childPopup->hasFocus()) {
        QWidget::focusOutEvent(event);
        return;
    }
    QWidget::focusOutEvent(event);
}

void NyanMenu::OnThemeChanged()
{
    RebuildRects();
    updateGeometry();
    update();
}

auto NyanMenu::PopupSize() const -> QSize
{
    QFont resolvedFont = font();
    if (const auto spec = Theme().Font("fontSM")) {
        resolvedFont.setPointSize(spec->sizeInPt);
    } else {
        resolvedFont.setPixelSize(12);
    }
    const QFontMetrics metrics(resolvedFont);
    const int itemPadding = Theme().DimensionPx("spaceSM").value_or(12);
    const int iconSize = Theme().DimensionPx("iconSizeSM").value_or(16);
    const int iconGap = Theme().DimensionPx("spaceXS").value_or(8);
    const int trailingGap = Theme().DimensionPx("spaceLG").value_or(24);
    int width = kPopupMinWidth;
    int height = Theme().DimensionPx("spaceXXS").value_or(4) * 2;

    for (const auto& runtime : _items) {
        const auto& item = runtime.data;
        if (!item.visible) {
            continue;
        }
        const int rowHeight = ItemHeight(item);
        height += rowHeight;

        const QString displayText = displayTextOf(item.text);
        const QString trailing = !item.extraText.isEmpty()
            ? item.extraText
            : item.shortcut.toString(QKeySequence::NativeText);
        const bool hasSubmenu = item.type == NyanMenuItemType::SubMenu || !item.children.isEmpty();
        const int submenuTrailingWidth = hasSubmenu ? Theme().DimensionPx("iconSizeXS").value_or(12) : 0;
        const int shortcutTrailingWidth = trailing.isEmpty() ? 0 : metrics.horizontalAdvance(trailing);
        const int trailingWidth = std::max(submenuTrailingWidth, shortcutTrailingWidth);
        const int rowWidth = itemPadding * 2
            + iconSize + iconGap
            + metrics.horizontalAdvance(displayText)
            + (trailingWidth > 0 ? trailingGap + trailingWidth : 0);
        width = std::max(width, rowWidth);
    }

    height = std::min(height, kPopupMaxHeight);
    return {width + kPopupShadowMargin * 2, height + kPopupShadowMargin * 2};
}

auto NyanMenu::ContentRect() const -> QRect
{
    return rect().adjusted(kPopupShadowMargin, kPopupShadowMargin, -kPopupShadowMargin, -kPopupShadowMargin);
}

auto NyanMenu::AvailableGeometry(const QPoint& pos) const -> QRect
{
    QScreen* screen = QGuiApplication::screenAt(pos);
    if (screen == nullptr) {
        screen = QGuiApplication::primaryScreen();
    }
    return screen != nullptr ? screen->availableGeometry() : QRect(pos, QSize(1024, 768));
}

auto NyanMenu::AdjustedGeometry(const QPoint& globalPos, NyanPopupPlacement placement) const -> QRect
{
    const QSize popupSize = PopupSize();
    QRect target(globalPos, popupSize);

    if (placement == NyanPopupPlacement::Below) {
        target.moveTop(globalPos.y() + kPopupOffset);
    } else if (placement == NyanPopupPlacement::Right) {
        target.moveLeft(globalPos.x() + kPopupOffset);
    } else if (placement == NyanPopupPlacement::Left) {
        target.moveRight(globalPos.x() - kPopupOffset);
    }

    const QRect available = AvailableGeometry(globalPos);
    if (target.right() > available.right()) {
        if (placement == NyanPopupPlacement::Right) {
            target.moveRight(globalPos.x() - kPopupOffset);
        } else {
            target.moveRight(available.right());
        }
    }
    if (target.bottom() > available.bottom()) {
        target.moveBottom(available.bottom());
    }
    if (target.left() < available.left()) {
        target.moveLeft(available.left());
    }
    if (target.top() < available.top()) {
        target.moveTop(available.top());
    }
    return target;
}

auto NyanMenu::CollapsedGeometry(const QRect& target,
                                 const QPoint& globalPos,
                                 NyanPopupPlacement placement) const -> QRect
{
    QRect start = target;
    constexpr int kMinimumExtent = 1;

    if (placement == NyanPopupPlacement::Right || placement == NyanPopupPlacement::Left) {
        start.setWidth(kMinimumExtent);
        const bool opensLeft = placement == NyanPopupPlacement::Left || target.left() < globalPos.x();
        if (opensLeft) {
            start.moveRight(target.right());
        }
        return start;
    }

    start.setHeight(kMinimumExtent);
    const bool opensUp = target.top() < globalPos.y();
    if (opensUp) {
        start.moveBottom(target.bottom());
    }
    return start;
}

auto NyanMenu::IndexAt(const QPoint& pos) const -> int
{
    for (int i = 0; i < _itemRects.size(); ++i) {
        if (_itemRects[i].contains(pos)) {
            return i;
        }
    }
    return -1;
}

auto NyanMenu::FirstActivatableIndex() const -> int
{
    for (int i = 0; i < _items.size(); ++i) {
        if (CanActivate(_items[i].data)) {
            return i;
        }
    }
    return -1;
}

auto NyanMenu::NextActivatableIndex(int from, int delta) const -> int
{
    if (_items.isEmpty()) {
        return -1;
    }

    int index = from;
    for (int step = 0; step < _items.size(); ++step) {
        index += delta;
        if (index < 0) {
            index = _items.size() - 1;
        } else if (index >= _items.size()) {
            index = 0;
        }
        if (CanActivate(_items[index].data)) {
            return index;
        }
    }
    return -1;
}

auto NyanMenu::CanActivate(const NyanMenuItemData& item) const -> bool
{
    return item.visible
        && item.enabled
        && item.type != NyanMenuItemType::Divider
        && item.type != NyanMenuItemType::Group;
}

auto NyanMenu::ItemHeight(const NyanMenuItemData& item) const -> int
{
    if (item.type == NyanMenuItemType::Divider) {
        return Theme().DimensionPx("spaceXS").value_or(8);
    }
    if (item.type == NyanMenuItemType::Group) {
        return Theme().DimensionPx("controlHeightSM").value_or(24);
    }
    return Theme().DimensionPx("controlHeightMD").value_or(32);
}

void NyanMenu::StartPopupAnimation(const QRect& target,
                                   const QPoint& globalPos,
                                   NyanPopupPlacement placement)
{
    StopPopupAnimation();

    if (kMotionDurationMs <= 0) {
        setWindowOpacity(1.0);
        setGeometry(target);
        return;
    }

    const QRect start = CollapsedGeometry(target, globalPos, placement);
    setWindowOpacity(0.0);
    setGeometry(start);

    auto* group = new QParallelAnimationGroup(this);
    auto* geometryAnimation = new QPropertyAnimation(this, "geometry", group);
    geometryAnimation->setDuration(kMotionDurationMs);
    geometryAnimation->setEasingCurve(QEasingCurve::OutCubic);
    geometryAnimation->setStartValue(start);
    geometryAnimation->setEndValue(target);

    auto* opacityAnimation = new QPropertyAnimation(this, "windowOpacity", group);
    opacityAnimation->setDuration(kMotionDurationMs);
    opacityAnimation->setEasingCurve(QEasingCurve::OutCubic);
    opacityAnimation->setStartValue(0.0);
    opacityAnimation->setEndValue(1.0);

    connect(group, &QParallelAnimationGroup::finished, this, [this, target]() {
        setGeometry(target);
        setWindowOpacity(1.0);
        _popupAnimation.clear();
    });

    _popupAnimation = group;
    group->start(QAbstractAnimation::DeleteWhenStopped);
}

void NyanMenu::StopPopupAnimation()
{
    if (_popupAnimation == nullptr) {
        return;
    }
    _popupAnimation->stop();
    _popupAnimation->deleteLater();
    _popupAnimation.clear();
}

void NyanMenu::RebuildRects()
{
    _itemRects.clear();
    _itemRects.reserve(_items.size());

    QRect content = ContentRect();
    const int verticalPadding = Theme().DimensionPx("spaceXXS").value_or(4);
    int y = content.top() + verticalPadding - _scrollOffset;
    for (const auto& runtime : _items) {
        const auto& item = runtime.data;
        if (!item.visible) {
            _itemRects.append(QRect{});
            continue;
        }
        const int height = ItemHeight(item);
        _itemRects.append(QRect(content.left() + Theme().DimensionPx("spaceXXS").value_or(4),
                                y,
                                std::max(0, content.width() - Theme().DimensionPx("spaceXXS").value_or(4) * 2),
                                height));
        y += height;
    }
}

void NyanMenu::PaintPopupFrame(QPainter& painter)
{
    const QRect content = ContentRect();
    const int radius = Theme().DimensionPx("radiusLarge").value_or(6);
    const QColor shadow = QColor(0, 10, 26, 26);
    const QColor bg = Theme().Color("colorBgContainer").value_or(QColor("#FFFFFFFF"));
    const QColor border = Theme().Color("colorBorder").value_or(QColor("#D8D8DA"));

    painter.setPen(Qt::NoPen);
    for (int i = 0; i < 4; ++i) {
        QColor layer = shadow;
        layer.setAlpha(std::max(0, shadow.alpha() - i * 5));
        painter.setBrush(layer);
        painter.drawRoundedRect(content.adjusted(-i - 1, i, i + 1, i + 2), radius + i, radius + i);
    }

    painter.setBrush(bg);
    painter.setPen(QPen(border, Theme().DimensionPx("lineWidthMS").value_or(1)));
    painter.drawRoundedRect(content.adjusted(0, 0, -1, -1), radius, radius);
}

void NyanMenu::PaintItem(QPainter& painter, const NyanMenuItemData& item, const QRect& rect, int index)
{
    if (!rect.intersects(this->rect())) {
        return;
    }

    const int itemPadding = Theme().DimensionPx("spaceSM").value_or(12);
    const int iconSize = Theme().DimensionPx("iconSizeSM").value_or(16);
    const int iconGap = Theme().DimensionPx("spaceXS").value_or(8);
    const int radius = Theme().DimensionPx("radiusDefault").value_or(3);
    const QColor divider = Theme().Color("colorDivider").value_or(QColor("#E3E3E6"));
    const QColor text = Theme().Color("colorText").value_or(QColor("#E0000000"));
    const QColor secondaryText = Theme().Color("colorTextSecondary").value_or(QColor("#A6000000"));
    const QColor disabledText = Theme().Color("colorTextTertiary").value_or(QColor("#66000000"));
    const QColor hoverBg = Theme().Color("colorFillHover").value_or(QColor("#0D000000"));
    const QColor pressedBg = Theme().Color("colorFillSecondaryHover").value_or(QColor("#1A000000"));
    const QColor selected = Theme().Color("colorPrimary").value_or(QColor("#0066FF"));
    const QColor selectedBg = Theme().Color("colorPrimaryBg").value_or(QColor("#F0F7FF"));
    const QColor danger = Theme().Color("colorError").value_or(QColor("#CC1423"));

    if (item.type == NyanMenuItemType::Divider) {
        const int y = rect.center().y();
        painter.setPen(QPen(divider, Theme().DimensionPx("lineWidthMS").value_or(1)));
        painter.drawLine(rect.left() + itemPadding, y, rect.right() - itemPadding, y);
        return;
    }

    const bool hovered = index == _hoveredIndex;
    const bool pressed = index == _pressedIndex;
    const bool active = index == _activeIndex;
    const bool interactive = CanActivate(item);

    if (interactive && (hovered || pressed || active)) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(pressed ? pressedBg : (active ? selectedBg : hoverBg));
        painter.drawRoundedRect(rect.adjusted(1, 2, -1, -2), radius, radius);
    }

    if (item.type == NyanMenuItemType::Group) {
        painter.setPen(secondaryText);
        painter.drawText(rect.adjusted(itemPadding, 0, -itemPadding, 0),
                         Qt::AlignVCenter | Qt::AlignLeft,
                         displayTextOf(item.text));
        return;
    }

    const QColor contentColor = !item.enabled ? disabledText
        : item.danger ? danger
        : active ? selected
        : text;

    int left = rect.left() + itemPadding;
    const int centerY = rect.center().y();
    const QRect iconRect(left, centerY - iconSize / 2, iconSize, iconSize);

    if (item.checkable) {
        painter.setPen(QPen(contentColor, 2));
        if (item.checked) {
            painter.drawLine(iconRect.left() + 3, iconRect.center().y(),
                             iconRect.left() + 7, iconRect.bottom() - 4);
            painter.drawLine(iconRect.left() + 7, iconRect.bottom() - 4,
                             iconRect.right() - 2, iconRect.top() + 4);
        }
    } else if (!item.icon.isNull()) {
        const QIcon::Mode mode = item.enabled ? QIcon::Normal : QIcon::Disabled;
        painter.drawPixmap(iconRect, item.icon.pixmap(iconRect.size(), mode));
    }
    left += iconSize + iconGap;

    const int right = rect.right() - itemPadding;
    const int trailingGap = Theme().DimensionPx("spaceLG").value_or(24);
    const int arrowSize = Theme().DimensionPx("iconSizeXS").value_or(12);
    const bool hasSubmenu = item.type == NyanMenuItemType::SubMenu || !item.children.isEmpty();
    const QString shortcut = !item.extraText.isEmpty()
        ? item.extraText
        : item.shortcut.toString(QKeySequence::NativeText);
    const int trailingWidth = hasSubmenu
        ? arrowSize
        : (shortcut.isEmpty() ? 0 : painter.fontMetrics().horizontalAdvance(shortcut));
    const int textRight = right - (trailingWidth > 0 ? trailingGap + trailingWidth : 0);
    const QRect textRect(left, rect.top(), std::max(0, textRight - left), rect.height());
    const QString displayText = displayTextOf(item.text);

    painter.setPen(contentColor);
    painter.drawText(textRect,
                     Qt::AlignVCenter | Qt::AlignLeft,
                     painter.fontMetrics().elidedText(displayText, Qt::ElideRight, textRect.width()));

    if (hasSubmenu) {
        const QRect arrowRect(right - arrowSize + 1, centerY - arrowSize / 2, arrowSize, arrowSize);
        painter.setPen(QPen(contentColor, 1.5));
        painter.drawLine(arrowRect.left() + 3, arrowRect.top() + 2,
                         arrowRect.right() - 3, arrowRect.center().y());
        painter.drawLine(arrowRect.right() - 3, arrowRect.center().y(),
                         arrowRect.left() + 3, arrowRect.bottom() - 2);
    } else {
        if (!shortcut.isEmpty()) {
            painter.setPen(item.enabled ? secondaryText : disabledText);
            painter.drawText(QRect(right - trailingWidth, rect.top(), trailingWidth, rect.height()),
                             Qt::AlignVCenter | Qt::AlignRight,
                             shortcut);
        }
    }
}

void NyanMenu::TriggerItem(int index)
{
    if (index < 0 || index >= _items.size() || !CanActivate(_items[index].data)) {
        return;
    }

    auto& runtime = _items[index];
    auto& item = runtime.data;
    if (item.type == NyanMenuItemType::SubMenu || !item.children.isEmpty() || runtime.submenuProxy != nullptr) {
        OpenSubmenu(index);
        return;
    }

    if (item.checkable) {
        item.checked = !item.checked;
        if (runtime.checkProxy != nullptr) {
            runtime.checkProxy->SetChecked(item.checked);
        }
    }
    if (item.action != nullptr) {
        item.action->trigger();
        Q_EMIT ActionTriggered(item.action);
    }
    if (runtime.itemProxy != nullptr) {
        Q_EMIT ItemTriggered(runtime.itemProxy);
    }
    Q_EMIT ItemKeyTriggered(item.key);
    Q_EMIT CloseRequested();

    NyanMenu* root = this;
    while (auto* parent = root->ParentMenu()) {
        root = parent;
    }
    root->CloseAll();
}

void NyanMenu::OpenSubmenu(int index)
{
    if (index < 0 || index >= _items.size()) {
        CloseChildPopup();
        return;
    }

    auto& runtime = _items[index];
    const auto& item = runtime.data;
    if (!CanActivate(item) || (item.children.isEmpty() && runtime.submenuProxy == nullptr)) {
        CloseChildPopup();
        return;
    }

    if (_itemRects.size() <= index || _itemRects[index].isEmpty()) {
        return;
    }

    NyanMenu* submenu = runtime.submenuProxy;
    if (submenu == nullptr) {
        submenu = new NyanMenu(this);
        submenu->_isSubmenu = true;
        submenu->SetItems(item.children);
        runtime.submenuProxy = submenu;
    } else if (!item.children.isEmpty()) {
        submenu->SetItems(item.children);
    }

    if (_childPopup != nullptr && _childPopup != submenu) {
        _childPopup->CloseAll();
    }

    _childPopup = submenu;
    const QPoint pos = mapToGlobal(_itemRects[index].topRight());
    Q_EMIT SubmenuRequested(item.key, _itemRects[index]);
    submenu->PopupBeside(pos, _itemRects[index].height());
}

void NyanMenu::CloseChildPopup()
{
    if (_submenuTimer != nullptr) {
        _submenuTimer->stop();
    }
    if (_childPopup != nullptr) {
        _childPopup->CloseAll();
        _childPopup.clear();
    }
    _pendingSubmenuIndex = -1;
}

void NyanMenu::MoveActive(int delta)
{
    if (_items.isEmpty()) {
        return;
    }

    const int start = _activeIndex >= 0 ? _activeIndex : (delta > 0 ? -1 : 0);
    const int next = NextActivatableIndex(start, delta);
    if (next >= 0) {
        _activeIndex = next;
        _hoveredIndex = next;
        update();
    }
}

void NyanMenu::SyncActionStates()
{
    for (auto& runtime : _items) {
        if (runtime.itemProxy != nullptr) {
            runtime.data.text = runtime.itemProxy->Text();
            runtime.data.icon = runtime.itemProxy->Icon();
            runtime.data.shortcut = runtime.itemProxy->Shortcut();
            runtime.data.enabled = runtime.itemProxy->isEnabled();
            if (runtime.data.key.isEmpty()) {
                runtime.data.key = runtime.data.text;
            }
        }
        if (runtime.checkProxy != nullptr) {
            runtime.data.checkable = true;
            runtime.data.checked = runtime.checkProxy->IsChecked();
        }
        if (runtime.data.action == nullptr) {
            continue;
        }
        auto* action = runtime.data.action.data();
        runtime.data.text = action->text();
        runtime.data.icon = action->icon();
        runtime.data.shortcut = action->shortcut();
        runtime.data.enabled = action->isEnabled();
        runtime.data.visible = action->isVisible();
        runtime.data.checkable = action->isCheckable();
        runtime.data.checked = action->isChecked();
    }
}

void NyanMenu::ConnectAction(QAction* action)
{
    if (action == nullptr) {
        return;
    }
    connect(action, &QAction::changed, this, [this]() {
        SyncActionStates();
        RebuildRects();
        updateGeometry();
        update();
    });
    connect(action, &QObject::destroyed, this, [this, action]() {
        for (auto& runtime : _items) {
            if (runtime.data.action == action) {
                runtime.data.action.clear();
                runtime.data.enabled = false;
            }
        }
        update();
    });
}

void NyanMenu::RequestSubmenuOpen(int index)
{
    if (index < 0 || index >= _items.size()) {
        if (_submenuTimer != nullptr) {
            _submenuTimer->stop();
        }
        _pendingSubmenuIndex = -1;
        return;
    }

    const auto& runtime = _items[index];
    const auto& item = runtime.data;
    if (!CanActivate(item) || (item.type != NyanMenuItemType::SubMenu && item.children.isEmpty() && runtime.submenuProxy == nullptr)) {
        if (_childPopup != nullptr) {
            CloseChildPopup();
        }
        return;
    }

    if (_childPopup != nullptr && _childPopup == runtime.submenuProxy) {
        return;
    }

    _pendingSubmenuIndex = index;
    _submenuTimer->start(kSubmenuDelayMs);
}

} // namespace matcha::gui
