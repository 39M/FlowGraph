---
title: Subgraph Navigation Breadcrumb
parent: Guides
nav_order: 99
---

# Subgraph Navigation Breadcrumb

## Problem

When a Level Designer double-clicks a **SubGraph node** in edit mode, the child asset opens in a new editor window. There is no indication of where you are in the asset hierarchy, and no way to quickly return to the parent graph.

Previously the `SFlowAssetBreadcrumb` widget only appeared during PIE (Play In Editor) mode, where it tracked the runtime instance chain. It was invisible during normal editing.

## Solution

Edit-mode breadcrumb navigation is now supported. When you double-click a SubGraph node to open its child asset:

1. The child asset editor shows a breadcrumb trail at the top of the graph area.
2. Clicking any ancestor in the breadcrumb opens that editor and selects the SubGraph node that leads to the child.

### Example

If your asset hierarchy is:

```
MainLevelFlow
  └── Chapter3        (SubGraph)
        └── Intro     (SubGraph, currently editing)
```

The breadcrumb displays:

```
► MainLevelFlow  ›  Chapter3  ›  Intro
```

Clicking **Chapter3** opens the Chapter3 editor and focuses the SubGraph node that points to Intro.

---

## Implementation Details

### Files Changed

| File | Change |
|------|--------|
| `Source/Flow/Public/FlowAsset.h` | Added `EditNavParents` (`TArray<TSoftObjectPtr<UFlowAsset>>`, `Transient`) to store the ancestor chain in edit mode |
| `Source/FlowEditor/Public/Asset/FlowAssetToolbar.h` | Removed `const` from `FFlowBreadcrumb` weak pointer fields so both PIE and edit-mode assets can share the struct |
| `Source/FlowEditor/Private/Graph/Nodes/FlowGraphNode.cpp` | In `OnNodeDoubleClicked()`, after opening a sub-asset in edit mode, copies the parent's `EditNavParents` + appends the parent asset itself to the child's `EditNavParents` |
| `Source/FlowEditor/Private/Asset/FlowAssetToolbar.cpp` | Updated `GetBreadcrumbVisibility()`, `FillBreadcrumb()`, and `OnCrumbClicked()` to handle both PIE and edit-mode paths |

### How Navigation State is Tracked

`EditNavParents` is a `Transient` `UPROPERTY` — it is **not serialized** and resets on editor restart. It is populated only when a user navigates into a subgraph by double-clicking a SubGraph node.

```
User double-clicks SubGraph node (edit mode)
  → UFlowGraphNode::OnNodeDoubleClicked()
      → OpenEditorForAsset(SubFlowAsset)
      → SubFlowAsset->EditNavParents = Parent->EditNavParents + [Parent]
  → SFlowAssetBreadcrumb::GetBreadcrumbVisibility() returns Visible
  → SFlowAssetBreadcrumb::FillBreadcrumb() builds trail from EditNavParents
```

### PIE Mode Unchanged

The existing runtime breadcrumb (walking `GetParentInstance()` on live instances) is completely unchanged. The edit-mode breadcrumb only activates when `GEditor->PlayWorld == nullptr`.
