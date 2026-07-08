#include <Matcha/Widgets/Menu/NyanMenuTypes.h>

#include <QAction>
#include <QMenu>

namespace matcha::gui {

auto NyanMenuItemData::ActionItem(const QString& key,
                                  const QString& text,
                                  const QKeySequence& shortcut,
                                  bool danger) -> NyanMenuItemData
{
    NyanMenuItemData item;
    item.key = key;
    item.text = text;
    item.shortcut = shortcut;
    item.danger = danger;
    item.type = NyanMenuItemType::Item;
    return item;
}

auto NyanMenuItemData::SubMenu(const QString& key,
                               const QString& text,
                               const QList<NyanMenuItemData>& children) -> NyanMenuItemData
{
    NyanMenuItemData item;
    item.key = key;
    item.text = text;
    item.children = children;
    item.type = NyanMenuItemType::SubMenu;
    return item;
}

auto NyanMenuItemData::Group(const QString& text) -> NyanMenuItemData
{
    NyanMenuItemData item;
    item.text = text;
    item.enabled = false;
    item.type = NyanMenuItemType::Group;
    return item;
}

auto NyanMenuItemData::Divider() -> NyanMenuItemData
{
    NyanMenuItemData item;
    item.enabled = false;
    item.type = NyanMenuItemType::Divider;
    return item;
}

auto NyanMenuItemData::FromAction(QAction* action) -> NyanMenuItemData
{
    NyanMenuItemData item;
    if (action == nullptr) {
        item.enabled = false;
        item.visible = false;
        return item;
    }

    item.key = action->objectName().isEmpty() ? action->text() : action->objectName();
    item.text = action->text();
    item.icon = action->icon();
    item.shortcut = action->shortcut();
    item.enabled = action->isEnabled();
    item.visible = action->isVisible();
    item.checkable = action->isCheckable();
    item.checked = action->isChecked();
    item.action = action;

    if (auto* menu = action->menu()) {
        item.type = NyanMenuItemType::SubMenu;
        for (auto* childAction : menu->actions()) {
            if (childAction != nullptr && childAction->isSeparator()) {
                item.children.append(Divider());
            } else {
                item.children.append(FromAction(childAction));
            }
        }
    }

    return item;
}

} // namespace matcha::gui
