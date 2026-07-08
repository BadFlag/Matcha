/**
 * @file NyanMainTitleBar.cpp
 * @brief 主窗口标题栏控件实现。
 */

#include <Matcha/Widgets/Shell/NyanMainTitleBar.h>

#include <Matcha/Theming/DesignTokens.h>
#include <Matcha/Widgets/Menu/NyanMenuBar.h>

#include <QApplication>
#include <QEvent>
#include <QGridLayout>
#include <QIcon>
#include <QMouseEvent>
#include <QPainter>
#include <QToolButton>
#include <QWindow>

#include <algorithm>
#include <array>

namespace matcha::gui {

NyanMainTitleBar::NyanMainTitleBar(QWidget* parent)
    : QWidget(parent)
    , ThemeAware(WidgetKind::MainTitleBar)
    , _hostWindow(parent)
{
    Q_ASSERT_X(parent != nullptr, "NyanMainTitleBar", "创建 NyanMainTitleBar 时必须指定父窗口。");
    if (parent == nullptr) {
        qFatal("NyanMainTitleBar requires a non-null parent window.");
    }

    setMouseTracking(true);
    setAttribute(Qt::WA_Hover, true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    // 监听标题栏、父窗口和宿主窗口内子控件，保证标题栏拖拽、窗口状态和边缘 resize 都能被捕获。
    installEventFilter(this);
    _hostWindow->installEventFilter(this);
    _hostWindow->setMouseTracking(true);
    if (qApp != nullptr) {
        qApp->installEventFilter(this);
    }

    InitLayout();
    ApplyTheme();
}

NyanMainTitleBar::~NyanMainTitleBar()
{
    if (_hostWindow != nullptr) {
        _hostWindow->removeEventFilter(this);
    }
    if (_customCentral != nullptr) {
        _customCentral->removeEventFilter(this);
    }
    if (qApp != nullptr) {
        qApp->removeEventFilter(this);
    }
    removeEventFilter(this);
}

void NyanMainTitleBar::InitLayout()
{
    _layout = new QGridLayout(this);
    _layout->setContentsMargins(0, 0, 0, 0);
    _layout->setSpacing(0);

    _menuBar = new NyanMenuBar(this);
    _layout->addWidget(_menuBar, 0, 1);
    _layout->setColumnStretch(2, 1);

    _minimizeButton = new QToolButton(this);
    _maximizeButton = new QToolButton(this);
    _closeButton = new QToolButton(this);
    int buttonColumn = 3;
    for (auto* button : {_minimizeButton, _maximizeButton, _closeButton}) {
        button->setAutoRaise(true);
        button->setFocusPolicy(Qt::NoFocus);
        button->setCursor(Qt::ArrowCursor);
        _layout->addWidget(button, 0, buttonColumn++);
    }

    connect(_minimizeButton, &QToolButton::clicked, this, [this]() {
        _hostWindow->showMinimized();
        Q_EMIT MinimizeRequested();
    });
    connect(_maximizeButton, &QToolButton::clicked, this, [this]() {
        _hostWindow->isMaximized() ? _hostWindow->showNormal() : _hostWindow->showMaximized();
        UpdateMaximizeButton();
        Q_EMIT MaximizeRequested();
    });
    connect(_closeButton, &QToolButton::clicked, this, [this]() {
        _hostWindow->close();
        Q_EMIT CloseRequested();
    });
}

void NyanMainTitleBar::SetTitleLogo(const QPixmap& titleLogo)
{
    _titleLogo = titleLogo;
    update();
}

auto NyanMainTitleBar::TitleLogo() const -> QPixmap
{
    return _titleLogo;
}

auto NyanMainTitleBar::MenuBar() -> NyanMenuBar*
{
    return _menuBar;
}

void NyanMainTitleBar::SetRouteBackEnabled(bool enabled)
{
    if (_routeBackEnabled == enabled) {
        return;
    }

    _routeBackEnabled = enabled;
    if (!_routeBackEnabled) {
        _routeBackHovered = false;
        _routeBackPressed = false;
    }
    update(RouteBackRect());
}

auto NyanMainTitleBar::IsRouteBackEnabled() const -> bool
{
    return _routeBackEnabled;
}

void NyanMainTitleBar::SetRouteBackIcon(const QIcon& icon)
{
    _routeBackIcon = icon;
    update(RouteBackRect());
}

auto NyanMainTitleBar::RouteBackIcon() const -> QIcon
{
    return _routeBackIcon;
}

void NyanMainTitleBar::SetRouteBackButtonShape(RouteBackButtonShape shape)
{
    if (_routeBackShape == shape) {
        return;
    }

    _routeBackShape = shape;
    update(RouteBackRect());
}

auto NyanMainTitleBar::RouteBackButtonShapeValue() const -> RouteBackButtonShape
{
    return _routeBackShape;
}

void NyanMainTitleBar::SetCustomCentral(QWidget* widget, bool autoDelete)
{
    if (_customCentral == widget) {
        return;
    }

    if (_customCentral != nullptr) {
        _layout->removeWidget(_customCentral);
        _customCentral->hide();
        _customCentral->removeEventFilter(this);
        _customCentral->setParent(nullptr);
        if (autoDelete) {
            _customCentral->deleteLater();
        }
    }

    _customCentral = widget;
    if (_customCentral != nullptr) {
        _customCentral->setParent(this);
        _customCentral->installEventFilter(this);
        _customCentral->setMouseTracking(true);
        _layout->addWidget(_customCentral, 1, 1, 1, 5);
        _customCentral->show();
    }
    ApplyTheme();
}

auto NyanMainTitleBar::CustomCentral() const -> QWidget*
{
    return _customCentral;
}

auto NyanMainTitleBar::sizeHint() const -> QSize
{
    return {800, Theme().DimensionPx("controlHeightSM").value_or(24)
        + Theme().DimensionPx("controlHeightXL").value_or(40)};
}

auto NyanMainTitleBar::minimumSizeHint() const -> QSize
{
    return {400, Theme().DimensionPx("controlHeightSM").value_or(24)
        + Theme().DimensionPx("controlHeightXL").value_or(40)};
}

void NyanMainTitleBar::ApplyTheme()
{
    const int upperHeight = Theme().DimensionPx("controlHeightSM").value_or(24);
    const int lowerHeight = Theme().DimensionPx("controlHeightXL").value_or(40);
    const int buttonWidth = Theme().DimensionPx("containerWidthLG").value_or(36);
    const int iconSize = Theme().DimensionPx("iconSizeSM").value_or(16);

    setFixedHeight(upperHeight + lowerHeight);
    if (_customCentral != nullptr) {
        _customCentral->setFixedHeight(lowerHeight);
    }
    _layout->setColumnMinimumWidth(0, RouteBackRect().right() + 1);
    _layout->setRowMinimumHeight(0, upperHeight);
    _layout->setRowMinimumHeight(1, lowerHeight);

    const QColor hover = Theme().Color("colorFillHover").value_or(QColor("#0D000000"));
    const QColor pressed = Theme().Color("colorFillSecondaryHover").value_or(QColor("#1A000000"));
    const QColor closeHover = Theme().Color("colorErrorHover").value_or(QColor("#D93840"));
    const QColor closePressed = Theme().Color("colorErrorActive").value_or(QColor("#A6081B"));

    const QString normalStyle = QStringLiteral(
        "QToolButton{border:none;background:transparent;padding:0;}"
        "QToolButton:hover{background:%1;}"
        "QToolButton:pressed{background:%2;}")
        .arg(hover.name(QColor::HexArgb), pressed.name(QColor::HexArgb));
    const QString closeStyle = QStringLiteral(
        "QToolButton{border:none;background:transparent;padding:0;}"
        "QToolButton:hover{background:%1;}"
        "QToolButton:pressed{background:%2;}")
        .arg(closeHover.name(QColor::HexArgb), closePressed.name(QColor::HexArgb));

    _minimizeButton->setStyleSheet(normalStyle);
    _maximizeButton->setStyleSheet(normalStyle);
    _closeButton->setStyleSheet(closeStyle);

    for (auto* button : {_minimizeButton, _maximizeButton, _closeButton}) {
        button->setFixedSize(buttonWidth, upperHeight);
        button->setIconSize(QSize(iconSize, iconSize));
    }

    const QColor iconColor = Theme().Color("colorText").value_or(QColor("#E0000000"));
    _minimizeButton->setIcon(QIcon(Theme().ResolveIcon(fw::IconId(fw::icons::Minimize), fw::IconToken::iconSizeSM, iconColor)));
    _closeButton->setIcon(QIcon(Theme().ResolveIcon(fw::IconId(fw::icons::Close), fw::IconToken::iconSizeSM, iconColor)));
    UpdateMaximizeButton();
    update();
}

void NyanMainTitleBar::UpdateMaximizeButton()
{
    const QColor iconColor = Theme().Color("colorText").value_or(QColor("#E0000000"));
    const auto icon = _hostWindow->isMaximized() ? fw::icons::Restore : fw::icons::Maximize;
    _maximizeButton->setIcon(QIcon(Theme().ResolveIcon(fw::IconId(icon), fw::IconToken::iconSizeSM, iconColor)));
}

auto NyanMainTitleBar::RouteBackRect() const -> QRect
{
    const int upperHeight = Theme().DimensionPx("controlHeightSM").value_or(24);
    const int lowerHeight = Theme().DimensionPx("controlHeightXL").value_or(40);
    const int leftOffset = Theme().DimensionPx("spaceXS").value_or(8);
    const int size = upperHeight + lowerHeight;
    return {leftOffset, 0, size, height() > 0 ? height() : size};
}

auto NyanMainTitleBar::IsRouteBackHit(const QPoint& pos) const -> bool
{
    return _routeBackEnabled && RouteBackRect().contains(pos);
}

void NyanMainTitleBar::PaintRouteBackButton(QPainter* painter)
{
    if (painter == nullptr) {
        return;
    }

    const QRect routeRect = RouteBackRect();
    if (routeRect.isEmpty()) {
        return;
    }

    const int buttonSize = routeRect.width();
    QRect buttonRect(
        routeRect.left() + (routeRect.width() - buttonSize) / 2,
        routeRect.top() + (routeRect.height() - buttonSize) / 2,
        buttonSize,
        buttonSize);
    buttonRect.adjust(2, 2, -2, -2);

    if (_routeBackEnabled || _routeBackShape == RouteBackButtonShape::Circle) {
        painter->save();
        painter->setPen(Qt::NoPen);

        QColor background = Theme().Color("colorPrimaryNav").value_or(QColor("#0059B3"));
        if (_routeBackEnabled && _routeBackHovered) {
            background = Theme().Color("colorBgContainer").value_or(QColor("#FFFFFFFF"));
            background.setAlpha(150);
        }
        painter->setBrush(background);

        if (_routeBackShape == RouteBackButtonShape::Circle) {
            painter->drawEllipse(buttonRect);
        } else {
            painter->drawRect(buttonRect);
        }
        painter->restore();
    }

    painter->setRenderHints(QPainter::SmoothPixmapTransform | QPainter::Antialiasing);

    const QColor iconColor = Theme().Color("colorText").value_or(QColor("#E0000000"));
    const int iconSide = Theme().DimensionPx("iconSizeSM").value_or(16) * 2;
    const QRect iconRect(
        buttonRect.center().x() - iconSide / 2,
        buttonRect.center().y() - iconSide / 2,
        iconSide,
        iconSide);

    const QIcon icon = _routeBackIcon.isNull()
        ? QIcon(Theme().ResolveIcon(fw::IconId(fw::icons::Home), fw::IconToken::iconSizeSM, iconColor))
        : _routeBackIcon;
    painter->drawPixmap(iconRect, icon.pixmap(iconRect.size()));
}

void NyanMainTitleBar::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const int upperHeight = Theme().DimensionPx("controlHeightSM").value_or(24);
    const QColor upperBg = Theme().Color("colorBgContainer").value_or(QColor("#FFFFFFFF"));
    const QColor lowerBg = Theme().Color("colorPrimaryNav").value_or(QColor("#0059B3"));
    const QColor divider = Theme().Color("colorDivider").value_or(QColor("#E3E3E6"));

    // 只绘制标题栏自身，不绘制父窗口背景或阴影。
    painter.fillRect(QRect(0, 0, width(), upperHeight), upperBg);
    painter.fillRect(QRect(0, upperHeight, width(), height() - upperHeight), lowerBg);
    painter.fillRect(QRect(0, height() - 1, width(), 1), divider);
    PaintRouteBackButton(&painter);

    if (!_titleLogo.isNull()) {
        const int leftReserved = RouteBackRect().right() + 1 + _menuBar->width();
        const int rightReserved = _minimizeButton->width() + _maximizeButton->width() + _closeButton->width();
        const int reserved = std::max(leftReserved, rightReserved);
        const QRect titleRect(reserved, 0, std::max(0, width() - reserved * 2), upperHeight);
        if (titleRect.width() > 0) {
            const int padding = Theme().DimensionPx("spaceXXS").value_or(4);
            const QSize maxLogoSize(titleRect.width(), std::max(0, upperHeight - padding * 2));
            const QPixmap logo = _titleLogo.scaled(maxLogoSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            const QPoint logoTopLeft(
                titleRect.x() + (titleRect.width() - logo.width()) / 2,
                titleRect.y() + (titleRect.height() - logo.height()) / 2);
            painter.drawPixmap(logoTopLeft, logo);
        }
    }
}

auto NyanMainTitleBar::eventFilter(QObject* watched, QEvent* event) -> bool
{
    if (event == nullptr || _hostWindow == nullptr) {
        return QWidget::eventFilter(watched, event);
    }

    if (watched == _hostWindow && event->type() == QEvent::WindowStateChange) {
        UpdateMaximizeButton();
        return false;
    }

    if (!IsHostRelatedWidget(watched)) {
        return QWidget::eventFilter(watched, event);
    }

    switch (event->type()) {
    case QEvent::MouseButtonPress: {
        auto* mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() != Qt::LeftButton) {
            return false;
        }

        const QPoint globalPos = mouse->globalPosition().toPoint();
        if (IsRouteBackHit(mapFromGlobal(globalPos))) {
            _routeBackPressed = true;
            _routeBackHovered = true;
            update(RouteBackRect());
            return true;
        }

        if (_hostWindow->isMaximized() || _hostWindow->isFullScreen()) {
            return false;
        }

        const Qt::Edges edges = ResizeEdgesAtGlobalPos(globalPos);
        if (edges != Qt::Edges{}) {
            if (QWindow* handle = _hostWindow->windowHandle(); handle != nullptr && handle->startSystemResize(edges)) {
                UpdateResizeCursor(edges);
                return true;
            }
            _mouseOperation = MouseOperation::Resize;
            _resizeEdges = edges;
            _pressGlobalPos = globalPos;
            _pressWindowGeometry = _hostWindow->geometry();
            _hostWindow->grabMouse();
            UpdateResizeCursor(edges);
            return true;
        }

        if (IsTitleDragArea(mapFromGlobal(globalPos))) {
            if (QWindow* handle = _hostWindow->windowHandle(); handle != nullptr && handle->startSystemMove()) {
                return true;
            }
            _mouseOperation = MouseOperation::Move;
            _pressGlobalPos = globalPos;
            _pressWindowTopLeft = _hostWindow->frameGeometry().topLeft();
            return true;
        }
        break;
    }
    case QEvent::MouseMove: {
        auto* mouse = static_cast<QMouseEvent*>(event);
        const QPoint globalPos = mouse->globalPosition().toPoint();
        const bool routeHovered = IsRouteBackHit(mapFromGlobal(globalPos));
        if (_routeBackHovered != routeHovered) {
            _routeBackHovered = routeHovered;
            update(RouteBackRect());
        }

        if (_mouseOperation == MouseOperation::Move) {
            _hostWindow->move(_pressWindowTopLeft + globalPos - _pressGlobalPos);
            return true;
        }
        if (_mouseOperation == MouseOperation::Resize) {
            ResizeHostWindow(globalPos);
            return true;
        }
        UpdateResizeCursor(ResizeEdgesAtGlobalPos(globalPos));
        break;
    }
    case QEvent::MouseButtonRelease: {
        auto* mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() == Qt::LeftButton && _routeBackPressed) {
            const bool contains = IsRouteBackHit(mapFromGlobal(mouse->globalPosition().toPoint()));
            _routeBackPressed = false;
            _routeBackHovered = contains;
            update(RouteBackRect());
            if (contains) {
                Q_EMIT RouteBackRequested();
            }
            return true;
        }
        const bool wasResizing = _mouseOperation == MouseOperation::Resize;
        _mouseOperation = MouseOperation::None;
        _resizeEdges = {};
        if (wasResizing) {
            _hostWindow->releaseMouse();
        }
        UpdateResizeCursor(ResizeEdgesAtGlobalPos(mouse->globalPosition().toPoint()));
        break;
    }
    case QEvent::MouseButtonDblClick: {
        auto* mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() == Qt::LeftButton && IsTitleDragArea(mapFromGlobal(mouse->globalPosition().toPoint()))) {
            _hostWindow->isMaximized() ? _hostWindow->showNormal() : _hostWindow->showMaximized();
            UpdateMaximizeButton();
            Q_EMIT MaximizeRequested();
            return true;
        }
        break;
    }
    default:
        break;
    }

    return QWidget::eventFilter(watched, event);
}

auto NyanMainTitleBar::IsTitleDragArea(const QPoint& pos) const -> bool
{
    const int upperHeight = Theme().DimensionPx("controlHeightSM").value_or(24);
    if (!QRect(0, 0, width(), upperHeight).contains(pos)) {
        return false;
    }

    // 返回区域、菜单和窗口按钮不作为拖拽区域。
    if (RouteBackRect().contains(pos)) {
        return false;
    }

    const std::array<QWidget*, 4> blockedWidgets{
        _menuBar,
        _minimizeButton,
        _maximizeButton,
        _closeButton
    };
    for (const QWidget* child : blockedWidgets) {
        if (child != nullptr && QRect(child->mapTo(this, QPoint(0, 0)), child->size()).contains(pos)) {
            return false;
        }
    }
    return true;
}

auto NyanMainTitleBar::ResizeEdgesAtGlobalPos(const QPoint& globalPos) const -> Qt::Edges
{
    const QPoint titlePos = mapFromGlobal(globalPos);
    if (rect().contains(titlePos) && !IsTitleDragArea(titlePos)) {
        return {};
    }
    return ResizeEdgesAt(_hostWindow->mapFromGlobal(globalPos));
}

auto NyanMainTitleBar::IsHostRelatedWidget(QObject* watched) const -> bool
{
    if (watched == this || watched == _hostWindow || watched == _customCentral) {
        return true;
    }

    const auto* widget = qobject_cast<const QWidget*>(watched);
    return widget != nullptr && widget->window() == _hostWindow;
}

void NyanMainTitleBar::UpdateResizeCursor(Qt::Edges edges)
{
    if (_hostWindow == nullptr || _hostWindow->isMaximized() || _hostWindow->isFullScreen()) {
        edges = {};
    }

    Qt::CursorShape cursor = Qt::ArrowCursor;
    if (edges == Qt::LeftEdge || edges == Qt::RightEdge) {
        cursor = Qt::SizeHorCursor;
    } else if (edges == Qt::TopEdge || edges == Qt::BottomEdge) {
        cursor = Qt::SizeVerCursor;
    } else if (edges == (Qt::LeftEdge | Qt::TopEdge) || edges == (Qt::RightEdge | Qt::BottomEdge)) {
        cursor = Qt::SizeFDiagCursor;
    } else if (edges == (Qt::RightEdge | Qt::TopEdge) || edges == (Qt::LeftEdge | Qt::BottomEdge)) {
        cursor = Qt::SizeBDiagCursor;
    }
    _hostWindow->setCursor(cursor);
}

auto NyanMainTitleBar::ResizeEdgesAt(const QPoint& hostPos) const -> Qt::Edges
{
    if (_hostWindow == nullptr || _hostWindow->isMaximized() || _hostWindow->isFullScreen()) {
        return {};
    }

    const int border = Theme().DimensionPx("spaceXS").value_or(8);
    const QRect rect = _hostWindow->rect();
    Qt::Edges edges;

    if (hostPos.x() >= 0 && hostPos.x() < border) {
        edges |= Qt::LeftEdge;
    } else if (hostPos.x() < rect.width() && hostPos.x() >= rect.width() - border) {
        edges |= Qt::RightEdge;
    }

    if (hostPos.y() >= 0 && hostPos.y() < border) {
        edges |= Qt::TopEdge;
    } else if (hostPos.y() < rect.height() && hostPos.y() >= rect.height() - border) {
        edges |= Qt::BottomEdge;
    }

    return edges;
}

void NyanMainTitleBar::ResizeHostWindow(const QPoint& globalPos)
{
    QRect geometry = _pressWindowGeometry;
    const QPoint delta = globalPos - _pressGlobalPos;
    const QSize minSize = _hostWindow->minimumSize();

    if (_resizeEdges.testFlag(Qt::LeftEdge)) {
        geometry.setLeft(std::min(geometry.left() + delta.x(), geometry.right() - minSize.width() + 1));
    }
    if (_resizeEdges.testFlag(Qt::RightEdge)) {
        geometry.setRight(std::max(geometry.right() + delta.x(), geometry.left() + minSize.width() - 1));
    }
    if (_resizeEdges.testFlag(Qt::TopEdge)) {
        geometry.setTop(std::min(geometry.top() + delta.y(), geometry.bottom() - minSize.height() + 1));
    }
    if (_resizeEdges.testFlag(Qt::BottomEdge)) {
        geometry.setBottom(std::max(geometry.bottom() + delta.y(), geometry.top() + minSize.height() - 1));
    }

    _hostWindow->setGeometry(geometry);
}

void NyanMainTitleBar::OnThemeChanged()
{
    ApplyTheme();
}

} // namespace matcha::gui
