---
title: Subgraph Navigation Breadcrumb
parent: Guides
nav_order: 99
---

# Subgraph Navigation Breadcrumb

## Overview

When editing nested Flow Graphs, the breadcrumb bar above the graph panel shows where the current asset sits in the hierarchy and lets you jump back to any ancestor in one click.

```
Main Level Flow  ›  Chapter 3  ›  Fade To Black
```

The breadcrumb appears in both **edit mode** (normal editing) and **PIE** (Play In Editor / debug mode).

---

## Behavior

| How the asset was opened | Parents found | Breadcrumb shown |
|--------------------------|---------------|-----------------|
| Double-clicked SubGraph node from a parent graph | exact nav path | `Parent › Current` |
| Double-clicked through multiple levels | full path | `Root › Mid › Current` |
| Opened directly from Content Browser (parent is loaded) | 1 | `Parent › Current` |
| Opened directly from Content Browser (multiple parents loaded) | N | `↑ N references ▾ › Current` |
| Opened directly from Content Browser (no parent loaded) | 0 | no breadcrumb |
| PIE: viewing inspected runtime instance | runtime chain | instance hierarchy |

**Multi-parent**: clicking `↑ N references ▾` opens a dropdown listing all known parent assets. Selecting one opens that editor and focuses the SubGraph node that references the current asset.

**Note on auto-discovery**: when an asset is opened directly (not via navigation), only parents that are already loaded in memory are discovered. The breadcrumb will appear automatically once a parent has been opened.

---

## Implementation

### Key Design Decision: State Lives on the Editor, Not the Asset

A SubGraph asset can be referenced by multiple parents (`Chapter1`, `Chapter3` both use `FadeToBlack`). Storing the navigation chain on the `UFlowAsset` would be shared state — navigating from `Chapter3` would overwrite the breadcrumb context for an already-open `Chapter1 → FadeToBlack` editor.

**Solution**: `EditNavParents` and `OnEditNavChanged` live on `FFlowAssetEditor`. Each editor window owns its own navigation context. UE guarantees a unique editor instance per asset (`FToolkitManager::FindEditorForAsset`), so "last navigation wins" is the correct and intuitive behavior for a single editor window.

### Timing: Why a Delegate Is Needed

When a user double-clicks a SubGraph node:

1. `OpenEditorForAsset(SubFlowAsset)` is called → child editor builds its toolbar → `SFlowAssetBreadcrumb::Construct` runs → `FillBreadcrumb()` runs → `EditNavParents` is **still empty** at this point
2. Back in `FlowGraphNode::OnNodeDoubleClicked`, we get the child editor and write `EditNavParents`
3. We call `ChildEditor->OnEditNavChanged.Broadcast()` → `SFlowAssetBreadcrumb::FillBreadcrumb` is re-invoked → breadcrumb now shows correctly

Without the delegate, the breadcrumb would always be empty on first navigation.

### Auto-Discovery (Content Browser open)

Uses `IAssetRegistry::GetReferencers(PackageName)` to find all packages that reference the current asset. Cross-references with already-loaded `UFlowAsset` objects (via `FAssetData::FastGetAsset(false)`) to avoid synchronous loads. Filters to those that contain a `UFlowNode_SubGraph` pointing to the current asset.

---

## Files Changed

| File | Change |
|------|--------|
| `Source/Flow/Public/FlowAsset.h` | Removed `EditNavParents` (was incorrectly placed on the asset) |
| `Source/FlowEditor/Public/Asset/FlowAssetEditor.h` | Added `EditNavParents` array and `OnEditNavChanged` delegate |
| `Source/FlowEditor/Public/Asset/FlowAssetToolbar.h` | `FFlowBreadcrumb`: added `AllParents` for multi-parent crumbs; `SFlowAssetBreadcrumb`: added `AssetEditor` parameter and `bHasParentsForDisplay` cache |
| `Source/FlowEditor/Private/Asset/FlowAssetToolbar.cpp` | `Construct`, `GetBreadcrumbVisibility`, `FillBreadcrumb`, `OnCrumbClicked`, `BuildDebuggerToolbar` updated; added `FindDirectParents`, `BuildEditModeCrumbs`, `NavigateToParent` helpers |
| `Source/FlowEditor/Private/Graph/Nodes/FlowGraphNode.cpp` | On SubGraph double-click in edit mode: writes `EditNavParents` to child editor and broadcasts `OnEditNavChanged` |
