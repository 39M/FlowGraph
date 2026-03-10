// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors
#pragma once

#include "Widgets/Input/SComboBox.h"
#include "Widgets/Navigation/SBreadcrumbTrail.h"

#include "FlowAsset.h"

class FFlowAssetEditor;
class UFlowAssetEditorContext;
class UToolMenu;

/**
 * Gathers all instances of given Flow Asset per given context.
 * Example: all instances for given client in the multiplayer game.
 */
struct FFlowAssetInstanceContext
{
	FText DisplayText;
	TArray<TSharedPtr<FObjectKey>> AssetInstances;

	FFlowAssetInstanceContext()
	{
	}

	explicit FFlowAssetInstanceContext(const FText& InDisplayText)
		: DisplayText(InDisplayText)
	{
	}
};

/**
 * List of all instances of given Flow Asset.
 */
class FLOWEDITOR_API SFlowAssetInstanceList : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SFlowAssetInstanceList)
	{
	}

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TWeakObjectPtr<UFlowAsset> InTemplateAsset);
	virtual ~SFlowAssetInstanceList() override;

	static EVisibility GetDebuggerVisibility();

protected:
	void RefreshInstances();

	EVisibility GetContextVisibility() const;
	TSharedRef<SWidget> OnGenerateContextWidget(TSharedPtr<FObjectKey> Item);
	void OnContextSelectionChanged(TSharedPtr<FObjectKey> SelectedItem, ESelectInfo::Type SelectionType);
	FText GetSelectedContextName() const;

	TSharedRef<SWidget> OnGenerateInstanceWidget(TSharedPtr<FObjectKey> Item) const;
	void OnInstanceSelectionChanged(TSharedPtr<FObjectKey> SelectedItem, ESelectInfo::Type SelectionType);
	FText GetSelectedInstanceName() const;
	FText JoinInstanceAndContextTexts(const FObjectKey& AssetInstance) const;

	TWeakObjectPtr<UFlowAsset> TemplateAsset;

	TSharedPtr<SComboBox<TSharedPtr<FObjectKey>>> ContextComboBox;
	TSharedPtr<SComboBox<TSharedPtr<FObjectKey>>> InstanceComboBox;

	TArray<TSharedPtr<FObjectKey>> Contexts;
	TArray<TSharedPtr<FObjectKey>> Instances;
	TMap<FObjectKey, FFlowAssetInstanceContext> InstancesPerContext;

	TSharedPtr<FObjectKey> NoContext;
	TSharedPtr<FObjectKey> SelectedContext;
	TSharedPtr<FObjectKey> SelectedInstance;
	
	static FText AllContextsText;
	static FText NoInstanceSelectedText;
};

/**
 * The kind of breadcrumbs that Flow Debugger uses.
 */
struct FLOWEDITOR_API FFlowBreadcrumb
{
	TWeakObjectPtr<UFlowAsset> CurrentInstance;
	TWeakObjectPtr<UFlowAsset> ChildInstance;

	// Populated only for multi-parent indicator crumbs (N > 1 parents reference this asset).
	// When non-empty, clicking the crumb shows a dropdown listing all parents.
	TArray<TWeakObjectPtr<UFlowAsset>> AllParents;

	bool IsMultiParent() const { return AllParents.Num() > 0; }

	FFlowBreadcrumb()
		: CurrentInstance(nullptr)
		, ChildInstance(nullptr)
	{
	}

	explicit FFlowBreadcrumb(const TWeakObjectPtr<UFlowAsset> InCurrentInstance, const TWeakObjectPtr<UFlowAsset> InChildInstance)
		: CurrentInstance(InCurrentInstance)
		, ChildInstance(InChildInstance)
	{
	}
};

/**
 * Widget displaying chain of breadcrumbs.
 */
class FLOWEDITOR_API SFlowAssetBreadcrumb : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SFlowAssetBreadcrumb)
	{
	}

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TWeakObjectPtr<UFlowAsset> InTemplateAsset, TWeakPtr<FFlowAssetEditor> InAssetEditor);

private:
	EVisibility GetBreadcrumbVisibility() const;
	void FillBreadcrumb() const;
	void BuildEditModeCrumbs(const TArray<TSoftObjectPtr<UFlowAsset>>& NavParents) const;
	void OnCrumbClicked(const FFlowBreadcrumb& Item) const;

	TWeakObjectPtr<UFlowAsset> TemplateAsset;
	TWeakPtr<FFlowAssetEditor> AssetEditor;
	TSharedPtr<SBreadcrumbTrail<FFlowBreadcrumb>> BreadcrumbTrail;

	// Cached flag set by FillBreadcrumb; read by GetBreadcrumbVisibility to avoid re-running
	// discovery logic on every Slate paint pass.
	mutable bool bHasParentsForDisplay = false;
};

/**
 * Flow-specific implementation of the asset editor toolbar.
 */
class FLOWEDITOR_API FFlowAssetToolbar : public TSharedFromThis<FFlowAssetToolbar>
{
public:
	explicit FFlowAssetToolbar(const TSharedPtr<FFlowAssetEditor> InAssetEditor, UToolMenu* ToolbarMenu);

private:
	void BuildAssetToolbar(UToolMenu* ToolbarMenu) const;
	static TSharedRef<SWidget> MakeDiffMenu(const UFlowAssetEditorContext* InContext);

	void BuildDebuggerToolbar(UToolMenu* ToolbarMenu) const;

private:
	TWeakPtr<FFlowAssetEditor> FlowAssetEditor;
};
