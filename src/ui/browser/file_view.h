#pragma once

#include "browser/browser_controller.h"
#include "model/details_columns.h"
#include "thumbnails/file_icon_cache.h"
#include "thumbnails/thumbnail_service.h"
#include "ui/controls/virtual_grid_view.h"

#include <chrono>
#include <optional>
#include <unordered_map>
#include <unordered_set>

class AnimationManager;

namespace luciole {
  //
  // Per-directory animation bookkeeping shared between the List and Grid
  // adapters so that switching view mode does not replay entry animations.
  //
  // BrowserPage owns one RevealState and passes a reference to both adapters.
  // FileViewAdapter (6-arg constructor) owns its own m_localReveal for use
  // in contexts that have no shared RevealState (e.g. unit tests).
  struct RevealState {
    // Indices that have already played their reveal animation. Grows as the
    // user scrolls; cleared by reset() on directory navigation.
    std::unordered_set<std::size_t> revealedIndices;
    // Captured at directory load time (or reset()), used to compute the
    // micro-stagger window (first 250 ms). The in-place initializer captures
    // construction time; reset() refreshes it.
    std::chrono::steady_clock::time_point loadTime = std::chrono::steady_clock::now();
    // Set by VirtualGridView when scroll velocity exceeds 3.5 viewports/sec.
    // Cleared automatically after 150 ms of idle scrolling.
    bool fastScroll = false;
    void reset() {
      revealedIndices.clear();
      loadTime = std::chrono::steady_clock::now();
      fastScroll = false;
    }
  };

  class FileViewAdapter final : public VirtualGridAdapter {
  public:
    FileViewAdapter(
        BrowserController& browser, FileIconCache& icons, ThumbnailService& thumbnails, ViewMode mode,
        std::function<std::uint32_t()> modifiers, std::function<bool()> focused
    );
    FileViewAdapter(
        BrowserController& browser, FileIconCache& icons, ThumbnailService& thumbnails, ViewMode mode,
        std::function<std::uint32_t()> modifiers, std::function<bool()> focused, RevealState& reveal,
        AnimationManager* animations
    );
    ~FileViewAdapter() override;
    std::size_t itemCount() const override;
    std::string itemTooltip(std::size_t index) const override;
    [[nodiscard]] std::unique_ptr<Node> createTile() override;
    std::unique_ptr<Node> createDecoration() override;
    void bindDecoration(Node& decoration, const VirtualGridDecorationLayout& layout) override;
    void beginTileLayout(bool fastScroll) override;
    void setTileVisible(Node& tile, bool visible) override;
    void releaseTile(Node& tile, Renderer& renderer) override;
    void destroyTile(Node& tile) override;
    void bindTile(Node& tile, std::size_t index, bool selected, bool hovered) override;
    bool onPointerPress(std::size_t index, float, float, float, float) override;
    bool onPointerDrag(std::optional<std::size_t>, float, float, float, float) override;
    PointerReleaseResult onPointerRelease(std::optional<std::size_t> index) override;
    void onPointerCancel() override;
    bool onBackgroundPointerPress() override;
    bool onAreaSelection(const std::vector<std::size_t>& indices) override;
    bool areaSelectionCurrent() const override;
    bool itemHitTest(std::size_t, float x, float y, float width, float height) const override;
    void onActivate(std::size_t index) override;
    void onSecondaryActivate(std::size_t index, float x, float y) override;
    void onBackgroundSecondaryActivate(float x, float y) override;
    void setContextCallback(std::function<void(std::optional<EntryId>, float, float)> callback);
    void setDragCallback(std::function<void(const std::vector<EntryId>&)> start, std::function<void()> cancel);
    void setActivateCallback(std::function<void(EntryId)> callback) { m_activate = std::move(callback); }
    void setCutPredicate(std::function<bool(const Location&)> predicate) { m_cut = std::move(predicate); }
    void setActivationMode(ActivationMode mode) { m_activationMode = mode; }
    void setPresentation(BrowsePresentation presentation, std::optional<bool> readOnly) {
      m_presentation = presentation;
      m_readOnly = readOnly;
    }
    void setGridIconSize(float size) { m_gridIconSize = size; }
    // List rows only; 0 keeps the default size.
    void setListIconSize(int size) { m_listIconSize = size; }
    void setDetailsColumns(DetailsColumns columns) { m_columns = columns; }
    void setDetailsColumnOrder(DetailsColumnOrder order) { m_columnOrder = order; }
    void setColumnActive(bool active) { m_columnActive = active; }
    void setPathEntry(std::optional<EntryId> entry) { m_pathEntry = entry; }
    void setAnimationManager(AnimationManager* animations);
    void resetReveal();

  private:
    BrowserController& m_browser;
    FileIconCache& m_icons;
    ThumbnailService& m_thumbnails;
    const ViewMode m_mode;
    bool m_columnActive = true;
    std::optional<EntryId> m_pathEntry;
    std::function<std::uint32_t()> m_modifiers;
    std::function<bool()> m_focused;
    std::function<bool(const Location&)> m_cut;
    std::optional<EntryId> m_pressed;
    std::optional<EntryId> m_expanderPress;
    std::optional<EntryId> m_lastClick;
    std::uint32_t m_pressModifiers = 0, m_lastClickModifiers = 0;
    std::chrono::steady_clock::time_point m_lastClickTime;
    std::function<void(EntryId)> m_activate;
    ActivationMode m_activationMode = ActivationMode::DoubleClick;
    bool m_dragged = false;
    bool m_activationClick = false;
    bool m_collapseSelectionOnClick = false;
    bool m_toggleSelectionOnClick = false;
    std::function<void(std::optional<EntryId>, float, float)> m_context;
    std::function<void(const std::vector<EntryId>&)> m_drag;
    std::function<void()> m_cancelDrag;
    BrowsePresentation m_presentation;
    std::optional<bool> m_readOnly;
    float m_gridIconSize = static_cast<float>(kDefaultGridIconSize);
    int m_listIconSize = 0;
    DetailsColumns m_columns;
    DetailsColumnOrder m_columnOrder = kDefaultDetailsColumnOrder;
    std::vector<EntryId> m_areaRows;
    std::unordered_set<EntryId> m_areaBaseline;
    std::uint64_t m_areaGeneration = 0;
    bool m_areaSelection = false;
    bool m_areaToggle = false;
    void startReveal(Node& tile, std::size_t index);
    void cancelTileAnimation(Node& tile);
    // Map from tile node pointer to the logical index it is currently animating.
    // Entries are added in startReveal() and removed in cancelTileAnimation()
    // and the onComplete callback. Used to detect recycled slots that are still
    // mid-animation for a different index.
    std::unordered_map<Node*, std::size_t> m_animatingTiles;
    // Tracks the current logical index bound to each tile node.
    std::unordered_map<Node*, std::size_t> m_tileIndices;
    // Fallback RevealState used by the 6-arg constructor when no shared state
    // is provided (e.g. unit tests, Miller columns).
    std::optional<RevealState> m_localReveal;
    // Non-owning pointer to the active RevealState. Always non-null after
    // construction; either points into m_localReveal or into BrowserPage.
    RevealState* m_reveal = nullptr;
    // Non-owning pointer to the application's AnimationManager. Null means
    // animations are disabled (6-arg constructor or scroll_reveal = false).
    AnimationManager* m_animations = nullptr;
  };
} // namespace luciole
