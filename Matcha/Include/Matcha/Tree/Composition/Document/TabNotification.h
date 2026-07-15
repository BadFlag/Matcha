#pragma once

/**
 * @file TabNotification.h
 * @brief Notification types for TabBarNode tab-page operations.
 *
 * These notifications are emitted by TabBarNode when adding a tab is requested,
 * tabs are switched, close-requested, dragged out, dropped in, or reordered.
 * They use PageId (StrongId) and live at the Document layer.
 */

#include "Matcha/Core/StrongId.h"
#include "Matcha/Event/Notification.h"

#include <string_view>

namespace matcha::fw {

// =========================================================================== //
//  Tab-page notifications (shared by TabBarNode variants)
// =========================================================================== //

class MATCHA_EXPORT TabPageAddRequested final : public Notification {
public:
    auto ClassName() const -> std::string_view override { return "TabPageAddRequested"; }
};

class MATCHA_EXPORT TabPageSwitched final : public Notification {
public:
    explicit TabPageSwitched(PageId pageId = {}) : _pageId(pageId) {}
    auto ClassName() const -> std::string_view override { return "TabPageSwitched"; }
    auto GetPageId() const -> PageId { return _pageId; }
private:
    PageId _pageId;
};

class MATCHA_EXPORT TabPageCloseRequested final : public Notification {
public:
    explicit TabPageCloseRequested(PageId pageId = {}) : _pageId(pageId) {}
    auto ClassName() const -> std::string_view override { return "TabPageCloseRequested"; }
    auto GetPageId() const -> PageId { return _pageId; }
private:
    PageId _pageId;
};

class MATCHA_EXPORT TabPageDraggedOut final : public Notification {
public:
    explicit TabPageDraggedOut(PageId pageId = {}, int globalX = 0, int globalY = 0)
        : _pageId(pageId), _globalX(globalX), _globalY(globalY) {}
    auto ClassName() const -> std::string_view override { return "TabPageDraggedOut"; }
    auto GetPageId() const -> PageId { return _pageId; }
    auto GlobalX() const -> int { return _globalX; }
    auto GlobalY() const -> int { return _globalY; }
private:
    PageId _pageId;
    int _globalX;
    int _globalY;
};

class MATCHA_EXPORT TabDroppedIn final : public Notification {
public:
    explicit TabDroppedIn(PageId pageId = {}, int insertIndex = -1)
        : _pageId(pageId), _insertIndex(insertIndex) {}
    auto ClassName() const -> std::string_view override { return "TabDroppedIn"; }
    auto GetPageId() const -> PageId { return _pageId; }
    auto InsertIndex() const -> int { return _insertIndex; }
private:
    PageId _pageId;
    int _insertIndex;
};

class MATCHA_EXPORT TabReordered final : public Notification {
public:
    explicit TabReordered(PageId pageId = {}, int oldIndex = -1, int newIndex = -1)
        : _pageId(pageId), _oldIndex(oldIndex), _newIndex(newIndex) {}
    auto ClassName() const -> std::string_view override { return "TabReordered"; }
    auto GetPageId() const -> PageId { return _pageId; }
    auto OldIndex() const -> int { return _oldIndex; }
    auto NewIndex() const -> int { return _newIndex; }
private:
    PageId _pageId;
    int _oldIndex;
    int _newIndex;
};

} // namespace matcha::fw
