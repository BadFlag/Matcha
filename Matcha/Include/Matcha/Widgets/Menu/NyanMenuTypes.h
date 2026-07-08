#pragma once

#include <Matcha/Core/Macros.h>

#include <QIcon>
#include <QKeySequence>
#include <QList>
#include <QPointer>
#include <QString>

class QAction;

namespace matcha::gui {

enum class NyanMenuItemType {
    Item,
    SubMenu,
    Group,
    Divider
};

enum class NyanPopupPlacement {
    Below,
    Context,
    Right,
    Left
};

struct MATCHA_EXPORT NyanMenuItemData {
    QString key;
    QString text;
    QIcon icon;
    QKeySequence shortcut;
    QString extraText;
    NyanMenuItemType type = NyanMenuItemType::Item;
    QList<NyanMenuItemData> children;
    bool enabled = true;
    bool visible = true;
    bool checkable = false;
    bool checked = false;
    bool danger = false;
    QPointer<QAction> action;

    [[nodiscard]] static auto ActionItem(const QString& key,
                                         const QString& text,
                                         const QKeySequence& shortcut = {},
                                         bool danger = false) -> NyanMenuItemData;
    [[nodiscard]] static auto SubMenu(const QString& key,
                                      const QString& text,
                                      const QList<NyanMenuItemData>& children) -> NyanMenuItemData;
    [[nodiscard]] static auto Group(const QString& text) -> NyanMenuItemData;
    [[nodiscard]] static auto Divider() -> NyanMenuItemData;
    [[nodiscard]] static auto FromAction(QAction* action) -> NyanMenuItemData;
};

} // namespace matcha::gui
