#pragma once

/**
 * @file NyanMainTitleBar.h
 * @brief 主窗口标题栏控件。
 */

#include <Matcha/Core/Macros.h>
#include <Matcha/Theming/ThemeAware.h>

#include <QIcon>
#include <QPixmap>
#include <QPoint>
#include <QRect>
#include <QWidget>

class QGridLayout;
class QPainter;
class QToolButton;

namespace matcha::gui {

class NyanMenuBar;

enum class RouteBackButtonShape {
    Rect,
    Circle
};

class MATCHA_EXPORT NyanMainTitleBar : public QWidget, public ThemeAware {
    Q_OBJECT

public:
    // 创建时必须指定父窗口。标题栏通过父窗口执行移动、缩放和窗口按钮动作。
    explicit NyanMainTitleBar(QWidget* parent);
    ~NyanMainTitleBar() override;

    NyanMainTitleBar(const NyanMainTitleBar&) = delete;
    auto operator=(const NyanMainTitleBar&) -> NyanMainTitleBar& = delete;
    NyanMainTitleBar(NyanMainTitleBar&&) = delete;
    auto operator=(NyanMainTitleBar&&) -> NyanMainTitleBar& = delete;

    // 设置上层标题区居中绘制的图片标题；不提供字符串标题接口。
    void SetTitleLogo(const QPixmap& titleLogo);
    [[nodiscard]] auto TitleLogo() const -> QPixmap;

    // 返回区域参考 AppTitleBar：手绘，点击只发出 RouteBackRequested。
    void SetRouteBackEnabled(bool enabled);
    [[nodiscard]] auto IsRouteBackEnabled() const -> bool;
    void SetRouteBackIcon(const QIcon& icon);
    [[nodiscard]] auto RouteBackIcon() const -> QIcon;
    void SetRouteBackButtonShape(RouteBackButtonShape shape);
    [[nodiscard]] auto RouteBackButtonShapeValue() const -> RouteBackButtonShape;

    [[nodiscard]] auto MenuBar() -> NyanMenuBar*;
    void SetCustomCentral(QWidget* widget, bool autoDelete = false);
    [[nodiscard]] auto CustomCentral() const -> QWidget*;

    [[nodiscard]] auto sizeHint() const -> QSize override;
    [[nodiscard]] auto minimumSizeHint() const -> QSize override;

Q_SIGNALS:
    void RouteBackRequested();
    void MinimizeRequested();
    void MaximizeRequested();
    void CloseRequested();

protected:
    [[nodiscard]] auto eventFilter(QObject* watched, QEvent* event) -> bool override;
    void paintEvent(QPaintEvent* event) override;
    void OnThemeChanged() override;

private:
    enum class MouseOperation {
        None,
        Move,
        Resize
    };

    void InitLayout();
    void ApplyTheme();
    void UpdateMaximizeButton();
    void PaintRouteBackButton(QPainter* painter);
    void ResizeHostWindow(const QPoint& globalPos);

    [[nodiscard]] auto RouteBackRect() const -> QRect;
    [[nodiscard]] auto IsRouteBackHit(const QPoint& pos) const -> bool;
    [[nodiscard]] auto IsTitleDragArea(const QPoint& pos) const -> bool;
    [[nodiscard]] auto ResizeEdgesAt(const QPoint& hostPos) const -> Qt::Edges;
    [[nodiscard]] auto ResizeEdgesAtGlobalPos(const QPoint& globalPos) const -> Qt::Edges;
    [[nodiscard]] auto IsHostRelatedWidget(QObject* watched) const -> bool;
    void UpdateResizeCursor(Qt::Edges edges);

private:
    QWidget* _hostWindow = nullptr;

    QGridLayout* _layout = nullptr;
    QWidget* _customCentral = nullptr;

    NyanMenuBar* _menuBar = nullptr;
    QToolButton* _minimizeButton = nullptr;
    QToolButton* _maximizeButton = nullptr;
    QToolButton* _closeButton = nullptr;

    QPixmap _titleLogo;
    bool _routeBackEnabled = false;
    bool _routeBackHovered = false;
    bool _routeBackPressed = false;
    QIcon _routeBackIcon;
    RouteBackButtonShape _routeBackShape = RouteBackButtonShape::Circle;

    // 只保留运行态数据，不缓存 Token 解析值。
    MouseOperation _mouseOperation = MouseOperation::None;
    Qt::Edges _resizeEdges;
    QPoint _pressGlobalPos;
    QPoint _pressWindowTopLeft;
    QRect _pressWindowGeometry;
};

} // namespace matcha::gui
