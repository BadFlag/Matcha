#include <Matcha/Widgets/Menu/NyanMenuBar.h>

#include <Matcha/Interaction/Focus/MnemonicManager.h>
#include <Matcha/Interaction/Focus/MnemonicState.h>
#include <Matcha/Widgets/Menu/NyanMenu.h>

#include <QAction>
#include <QApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>

#include <algorithm>

namespace matcha::gui {

NyanMenuBar::NyanMenuBar(QWidget* parent)
    : QWidget(parent)
    , ThemeAware(WidgetKind::MenuBar)
{
    setFixedHeight(BarHeight());
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);

    qApp->installEventFilter(this);

    if (auto* state = GetMnemonicState()) {
        connect(state, &MnemonicState::UnderlineVisibilityChanged,
                this, QOverload<>::of(&QWidget::update));
    }
}

NyanMenuBar::~NyanMenuBar()
{
    UnregisterMenuMnemonics();
    qApp->removeEventFilter(this);
}

void NyanMenuBar::SetItems(const QList<NyanMenuItemData>& items)
{
    Clear();
    for (const auto& item : items) {
        AddMenu(item);
    }
}

auto NyanMenuBar::Items() const -> QList<NyanMenuItemData>
{
    QList<NyanMenuItemData> result;
    result.reserve(_menus.size());
    for (const auto& entry : _menus) {
        result.append(entry.data);
    }
    return result;
}

void NyanMenuBar::AddMenu(const NyanMenuItemData& menu)
{
    MenuEntry entry;
    entry.data = menu;
    entry.rawTitle = menu.text;
    ParseTitle(entry, menu.text);
    entry.menu = new NyanMenu(this);
    entry.menu->SetItems(menu.children);
    HookMenuSignals(entry.menu);

    _menus.append(entry);
    RebuildRects();
    RegisterMenuMnemonics();
    updateGeometry();
    update();
    Q_EMIT ItemsChanged();
}

void NyanMenuBar::AddMenu(const QString& key, const QString& text, const QList<NyanMenuItemData>& children)
{
    AddMenu(NyanMenuItemData::SubMenu(key, text, children));
}

auto NyanMenuBar::AddMenu(const QString& title) -> NyanMenu*
{
    MenuEntry entry;
    entry.data = NyanMenuItemData::SubMenu(title, title, {});
    entry.rawTitle = title;
    ParseTitle(entry, title);
    entry.menu = new NyanMenu(this);
    HookMenuSignals(entry.menu);

    _menus.append(entry);
    RebuildRects();
    RegisterMenuMnemonics();
    updateGeometry();
    update();
    Q_EMIT ItemsChanged();
    return entry.menu;
}

void NyanMenuBar::RemoveMenu(NyanMenu* menu)
{
    const int index = IndexOfMenu(menu);
    if (index < 0) {
        return;
    }

    if (_activeIndex == index) {
        CloseActiveMenu();
    }
    delete _menus[index].menu;
    _menus.removeAt(index);
    _hoveredIndex = -1;
    _pressedIndex = -1;
    _activeIndex = -1;
    _menuOpen = false;
    RebuildRects();
    RegisterMenuMnemonics();
    updateGeometry();
    update();
    Q_EMIT ItemsChanged();
}

void NyanMenuBar::Clear()
{
    CloseActiveMenu();
    for (const auto& entry : _menus) {
        delete entry.menu;
    }
    _menus.clear();
    _itemRects.clear();
    _hoveredIndex = -1;
    _pressedIndex = -1;
    _activeIndex = -1;
    _menuOpen = false;
    RegisterMenuMnemonics();
    updateGeometry();
    update();
    Q_EMIT ItemsChanged();
}

auto NyanMenuBar::MenuAt(int index) const -> NyanMenu*
{
    if (index < 0 || index >= _menus.size()) {
        return nullptr;
    }
    return _menus[index].menu;
}

auto NyanMenuBar::MenuCount() const -> int
{
    return _menus.size();
}

auto NyanMenuBar::sizeHint() const -> QSize
{
    const_cast<NyanMenuBar*>(this)->RebuildRects();
    int width = Theme().DimensionPx("spaceXS").value_or(8) * 2;
    for (const QRect& rect : _itemRects) {
        if (!rect.isEmpty()) {
            width = std::max(width, rect.right() + Theme().DimensionPx("spaceXS").value_or(8));
        }
    }
    return {width, BarHeight()};
}

auto NyanMenuBar::minimumSizeHint() const -> QSize
{
    return {Theme().DimensionPx("containerWidthXS").value_or(20), BarHeight()};
}

void NyanMenuBar::paintEvent(QPaintEvent* /*event*/)
{
    SyncActionStates();
    RebuildRects();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QFont resolvedFont = font();
    if (const auto spec = Theme().Font("fontSM")) {
        resolvedFont.setPointSize(spec->sizeInPt);
    } else {
        resolvedFont.setPointSize(9);
    }
    painter.setFont(resolvedFont);
    painter.fillRect(rect(), Theme().Color("colorBgContainer").value_or(QColor("#FFFFFFFF")));

    for (int i = 0; i < _menus.size(); ++i) {
        PaintEntry(painter, i);
    }
}

void NyanMenuBar::mouseMoveEvent(QMouseEvent* event)
{
    const int index = IndexAt(event->pos());
    if (index != _hoveredIndex) {
        _hoveredIndex = index;
        update();
    }
    if (_menuOpen && index >= 0 && index != _activeIndex) {
        OpenMenu(index);
    }
    QWidget::mouseMoveEvent(event);
}

void NyanMenuBar::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        const int index = IndexAt(event->pos());
        if (index >= 0 && CanActivate(_menus[index])) {
            _pressedIndex = index;
            update();
        }
    }
    QWidget::mousePressEvent(event);
}

void NyanMenuBar::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        const int index = IndexAt(event->pos());
        const int pressed = _pressedIndex;
        _pressedIndex = -1;
        if (index >= 0 && index == pressed) {
            ToggleMenu(index);
        }
        update();
    }
    QWidget::mouseReleaseEvent(event);
}

void NyanMenuBar::leaveEvent(QEvent* event)
{
    _hoveredIndex = -1;
    _pressedIndex = -1;
    update();
    QWidget::leaveEvent(event);
}

void NyanMenuBar::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_Left:
        if (_menuOpen) {
            NavigateMenu(-1);
            event->accept();
            return;
        }
        break;
    case Qt::Key_Right:
        if (_menuOpen) {
            NavigateMenu(1);
            event->accept();
            return;
        }
        break;
    case Qt::Key_Down:
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Space: {
        int index = _activeIndex >= 0 ? _activeIndex : (_hoveredIndex >= 0 ? _hoveredIndex : FirstActivatableIndex());
        if (index >= 0) {
            OpenMenu(index);
            event->accept();
            return;
        }
        break;
    }
    case Qt::Key_Escape:
        CloseActiveMenu();
        event->accept();
        return;
    default:
        break;
    }
    QWidget::keyPressEvent(event);
}

bool NyanMenuBar::eventFilter(QObject* /*watched*/, QEvent* event)
{
    if (_menuOpen && event->type() == QEvent::MouseMove) {
        auto* mouseEvent = dynamic_cast<QMouseEvent*>(event);
        if (mouseEvent != nullptr) {
            const QPoint local = mapFromGlobal(mouseEvent->globalPosition().toPoint());
            const int index = IndexAt(local);
            if (index >= 0 && index != _activeIndex) {
                OpenMenu(index);
                return false;
            }
        }
    }

    if (event->type() == QEvent::KeyPress) {
        auto* keyEvent = dynamic_cast<QKeyEvent*>(event);
        const int key = keyEvent->key();

        if (key == Qt::Key_Alt || key == Qt::Key_F10) {
            _altPressedAlone = true;
            if (auto* state = GetMnemonicState()) {
                state->SetAltHeld(true);
            }
            return false;
        }

        if (key == Qt::Key_Escape) {
            if (auto* state = GetMnemonicState()) {
                if (state->IsAltActivated()) {
                    state->Deactivate();
                    return true;
                }
            }
        }

        if (keyEvent->modifiers() & Qt::AltModifier) {
            _altPressedAlone = false;
        }

        if ((keyEvent->modifiers() == Qt::AltModifier || (GetMnemonicState() && GetMnemonicState()->IsAltActivated()))
            && !keyEvent->text().isEmpty()) {
            if (auto* manager = fw::GetMnemonicManager()) {
                const auto ch = keyEvent->text().at(0).unicode();
                if (manager->Dispatch(ch)) {
                    if (auto* state = GetMnemonicState()) {
                        state->SetAltHeld(false);
                        state->Deactivate();
                    }
                    return true;
                }
            }
        }
    } else if (event->type() == QEvent::KeyRelease) {
        auto* keyEvent = dynamic_cast<QKeyEvent*>(event);
        const int key = keyEvent->key();
        if (key == Qt::Key_Alt || key == Qt::Key_F10) {
            if (auto* state = GetMnemonicState()) {
                state->SetAltHeld(false);
                if (_altPressedAlone && !keyEvent->isAutoRepeat()) {
                    state->SetAltActivated(!state->IsAltActivated());
                }
            }
            _altPressedAlone = false;
        }
    }

    return false;
}

void NyanMenuBar::OnThemeChanged()
{
    setFixedHeight(BarHeight());
    RebuildRects();
    updateGeometry();
    update();
}

auto NyanMenuBar::BarHeight() const -> int
{
    return Theme().DimensionPx("controlHeightSM").value_or(24);
}

auto NyanMenuBar::IndexAt(const QPoint& pos) const -> int
{
    for (int i = 0; i < _itemRects.size(); ++i) {
        if (_itemRects[i].contains(pos)) {
            return i;
        }
    }
    return -1;
}

auto NyanMenuBar::FirstActivatableIndex() const -> int
{
    for (int i = 0; i < _menus.size(); ++i) {
        if (CanActivate(_menus[i])) {
            return i;
        }
    }
    return -1;
}

auto NyanMenuBar::NextActivatableIndex(int from, int delta) const -> int
{
    if (_menus.isEmpty()) {
        return -1;
    }

    int index = from;
    for (int step = 0; step < _menus.size(); ++step) {
        index += delta;
        if (index < 0) {
            index = _menus.size() - 1;
        } else if (index >= _menus.size()) {
            index = 0;
        }
        if (CanActivate(_menus[index])) {
            return index;
        }
    }
    return -1;
}

auto NyanMenuBar::IndexOfMenu(NyanMenu* menu) const -> int
{
    for (int i = 0; i < _menus.size(); ++i) {
        if (_menus[i].menu == menu) {
            return i;
        }
    }
    return -1;
}

auto NyanMenuBar::CanActivate(const MenuEntry& entry) const -> bool
{
    return entry.data.visible && entry.data.enabled && entry.menu != nullptr;
}

void NyanMenuBar::ParseTitle(MenuEntry& entry, const QString& title)
{
    const auto parsed = MnemonicState::Parse(title);
    entry.rawTitle = title;
    entry.displayText = parsed.displayText;
    entry.mnemonic = parsed.mnemonicChar;
    entry.data.text = parsed.displayText;
    if (entry.data.key.isEmpty()) {
        entry.data.key = parsed.displayText;
    }
}

void NyanMenuBar::RebuildRects()
{
    _itemRects.clear();
    _itemRects.reserve(_menus.size());

    QFont resolvedFont = font();
    if (const auto spec = Theme().Font("fontSM")) {
        resolvedFont.setPointSize(spec->sizeInPt);
    } else {
        resolvedFont.setPointSize(9);
    }
    const QFontMetrics metrics(resolvedFont);
    const int gap = Theme().DimensionPx("spaceXXS").value_or(4);
    const int padding = Theme().DimensionPx("spaceXS").value_or(8);
    int x = padding;

    for (const auto& entry : _menus) {
        if (!entry.data.visible) {
            _itemRects.append(QRect{});
            continue;
        }
        const int width = metrics.horizontalAdvance(entry.displayText) + padding * 2;
        _itemRects.append(QRect(x, 0, width, BarHeight()));
        x += width + gap;
    }
}

void NyanMenuBar::PaintEntry(QPainter& painter, int index)
{
    if (index < 0 || index >= _menus.size() || index >= _itemRects.size()) {
        return;
    }

    const auto& entry = _menus[index];
    const QRect itemRect = _itemRects[index];
    if (!entry.data.visible || itemRect.isEmpty()) {
        return;
    }

    const bool enabled = CanActivate(entry);
    const bool hovered = index == _hoveredIndex;
    const bool pressed = index == _pressedIndex;
    const bool active = index == _activeIndex && _menuOpen;
    const QColor text = Theme().Color("colorText").value_or(QColor("#E0000000"));
    const QColor disabledText = Theme().Color("colorTextTertiary").value_or(QColor("#66000000"));
    const QColor hoverBg = Theme().Color("colorFillHover").value_or(QColor("#0D000000"));
    const QColor pressedBg = Theme().Color("colorFillSecondaryHover").value_or(QColor("#1A000000"));
    const QColor primary = Theme().Color("colorPrimary").value_or(QColor("#0066FF"));
    const int radius = Theme().DimensionPx("radiusDefault").value_or(3);
    const int lineHeight = Theme().DimensionPx("lineWidthMD").value_or(2);
    const int padding = Theme().DimensionPx("spaceXS").value_or(8);

    if (enabled && (hovered || pressed)) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(pressed ? pressedBg : hoverBg);
        painter.drawRoundedRect(itemRect.adjusted(0, 3, 0, -3), radius, radius);
    }

    painter.setPen(!enabled ? disabledText : (active ? primary : text));
    const bool showMnemonic = GetMnemonicState() != nullptr && GetMnemonicState()->ShouldShowUnderline();
    MnemonicState::DrawMnemonicText(painter,
                                    itemRect,
                                    Qt::AlignCenter,
                                    entry.rawTitle,
                                    showMnemonic);

    if (enabled && active) {
        const QRect lineRect(itemRect.left() + padding,
                             itemRect.bottom() - lineHeight + 1,
                             std::max(0, itemRect.width() - padding * 2),
                             lineHeight);
        painter.fillRect(lineRect, primary);
    }
}

void NyanMenuBar::OpenMenu(int index)
{
    if (index < 0 || index >= _menus.size() || !CanActivate(_menus[index])) {
        return;
    }

    _switchingMenu = true;
    CloseActiveMenu();
    _switchingMenu = false;

    auto& entry = _menus[index];
    if (entry.data.action != nullptr && entry.menu->ItemCount() == 0) {
        entry.data.action->trigger();
        Q_EMIT ActionTriggered(entry.data.action);
        Q_EMIT ItemTriggered(entry.data.key);
        return;
    }

    _activeIndex = index;
    _menuOpen = true;
    Q_EMIT MenuAboutToShow(entry.menu);

    const QPoint pos = mapToGlobal(_itemRects[index].bottomLeft());
    entry.menu->PopupBelow(pos, _itemRects[index].width());
    update();
}

void NyanMenuBar::CloseActiveMenu()
{
    if (_activeIndex >= 0 && _activeIndex < _menus.size() && _menus[_activeIndex].menu != nullptr) {
        _menus[_activeIndex].menu->CloseAll();
    }
    _menuOpen = false;
    _activeIndex = -1;
    update();
}

void NyanMenuBar::ToggleMenu(int index)
{
    if (_menuOpen && _activeIndex == index) {
        CloseActiveMenu();
        return;
    }

    if (_dismissedIndex == index && _dismissTimer.isValid() && _dismissTimer.elapsed() < 300) {
        _dismissedIndex = -1;
        return;
    }

    OpenMenu(index);
}

void NyanMenuBar::NavigateMenu(int delta)
{
    const int start = _activeIndex >= 0 ? _activeIndex : FirstActivatableIndex();
    const int next = NextActivatableIndex(start, delta);
    if (next >= 0) {
        OpenMenu(next);
    }
}

void NyanMenuBar::RegisterMenuMnemonics()
{
    UnregisterMenuMnemonics();

    auto* manager = fw::GetMnemonicManager();
    if (manager == nullptr) {
        return;
    }

    for (int i = 0; i < _menus.size(); ++i) {
        if (_menus[i].mnemonic.isNull()) {
            continue;
        }
        const int index = i;
        const uint64_t id = manager->Register({
            fw::MnemonicScope::Global,
            _menus[i].mnemonic.toLower().unicode(),
            [this, index]() { OpenMenu(index); },
            {}
        });
        _mnemonicIds.push_back(id);
    }
}

void NyanMenuBar::UnregisterMenuMnemonics()
{
    auto* manager = fw::GetMnemonicManager();
    if (manager == nullptr) {
        return;
    }

    for (uint64_t id : _mnemonicIds) {
        manager->Unregister(id);
    }
    _mnemonicIds.clear();
}

void NyanMenuBar::SyncActionStates()
{
    for (auto& entry : _menus) {
        if (entry.data.action == nullptr) {
            continue;
        }
        auto* action = entry.data.action.data();
        entry.data.enabled = action->isEnabled();
        entry.data.visible = action->isVisible();
        entry.data.icon = action->icon();
        entry.data.text = action->text();
        ParseTitle(entry, action->text());
    }
}

void NyanMenuBar::HookMenuSignals(NyanMenu* menu)
{
    if (menu == nullptr) {
        return;
    }

    connect(menu, &NyanMenu::AboutToHide, this, [this, menu]() {
        if (_switchingMenu) {
            return;
        }
        _dismissedIndex = IndexOfMenu(menu);
        _dismissTimer.start();
        _menuOpen = false;
        _activeIndex = -1;
        update();
    });
    connect(menu, &NyanMenu::ItemKeyTriggered, this, &NyanMenuBar::ItemTriggered);
    connect(menu, &NyanMenu::ActionTriggered, this, &NyanMenuBar::ActionTriggered);
}

} // namespace matcha::gui
