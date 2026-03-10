// Copyright https://github.com/MothCocoon/FlowGraph/graphs/contributors

#include "Asset/FlowAssetToolbar.h"

#include "Graph/FlowGraphUtils.h"
#include "Asset/FlowAssetEditor.h"
#include "Asset/FlowAssetEditorContext.h"
#include "Asset/SAssetRevisionMenu.h"
#include "FlowEditorCommands.h"

#include "FlowAsset.h"
#include "Nodes/Graph/FlowNode_SubGraph.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Kismet2/DebuggerCommands.h"
#include "Misc/Attribute.h"
#include "Misc/MessageDialog.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "ToolMenu.h"
#include "ToolMenuSection.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "SourceControlHelpers.h"

#define LOCTEXT_NAMESPACE "FlowDebuggerToolbar"

//////////////////////////////////////////////////////////////////////////
// Flow Asset Instance List

FText SFlowAssetInstanceList::NoInstanceSelectedText = LOCTEXT("NoInstanceSelected", "No instance selected");
FText SFlowAssetInstanceList::AllContextsText = LOCTEXT("All", "All");

void SFlowAssetInstanceList::Construct(const FArguments& InArgs, const TWeakObjectPtr<UFlowAsset> InTemplateAsset)
{
	TemplateAsset = InTemplateAsset;

	if (TemplateAsset.IsValid())
	{
		TemplateAsset->OnDebuggerRefresh().AddSP(this, &SFlowAssetInstanceList::RefreshInstances);
		RefreshInstances();
	}

	ContextComboBox = SNew(SComboBox<TSharedPtr<FObjectKey>>)
		.OptionsSource(&Contexts)
		.Visibility(this, &SFlowAssetInstanceList::GetContextVisibility)
		.OnGenerateWidget(this, &SFlowAssetInstanceList::OnGenerateContextWidget)
		.OnSelectionChanged(this, &SFlowAssetInstanceList::OnContextSelectionChanged)
		.ContentPadding(FMargin(0.f, 2.f))
		[
			SNew(STextBlock)
			.Text(this, &SFlowAssetInstanceList::GetSelectedContextName)
		];

	InstanceComboBox = SNew(SComboBox<TSharedPtr<FObjectKey>>)
		.OptionsSource(&Instances)
		.OnGenerateWidget(this, &SFlowAssetInstanceList::OnGenerateInstanceWidget)
		.OnSelectionChanged(this, &SFlowAssetInstanceList::OnInstanceSelectionChanged)
		.ContentPadding(FMargin(0.f, 2.f))
		[
			SNew(STextBlock)
			.Text(this, &SFlowAssetInstanceList::GetSelectedInstanceName)
		];

	ChildSlot
	[
		SNew(SHorizontalBox)
		.Visibility_Static(&SFlowAssetInstanceList::GetDebuggerVisibility)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0.0f, 0.0f, 8.0f, 0.0f)
		[
			ContextComboBox.ToSharedRef()
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0.0f, 0.0f, 4.0f, 0.0f)
		[
			InstanceComboBox.ToSharedRef()
		]
	];
}

SFlowAssetInstanceList::~SFlowAssetInstanceList()
{
	if (TemplateAsset.IsValid())
	{
		TemplateAsset->OnDebuggerRefresh().RemoveAll(this);
	}
}

EVisibility SFlowAssetInstanceList::GetDebuggerVisibility()
{
	return GEditor->PlayWorld ? EVisibility::Visible : EVisibility::Collapsed;
}

void SFlowAssetInstanceList::RefreshInstances()
{
	Contexts.Empty();
	Instances.Empty();
	InstancesPerContext.Empty();

	if (GEditor->ShouldEndPlayMap())
	{
		// prevent redundant refreshing list for every asset instance being destroyed
		return;
	}

	// add empty context, users sees this as the default "All" option 
	NoContext = MakeShareable(new FObjectKey(nullptr));
	Contexts.Add(NoContext);

	// add empty instance as default
	Instances.Add(MakeShareable(new FObjectKey(nullptr)));

	// gather all instances of given UFlowAsset
	for (const UFlowAsset* ActiveInstance : TemplateAsset->GetActiveInstances())
	{
		Instances.Add(MakeShareable(new FObjectKey(ActiveInstance)));

		// support World context in case of online multiplayer
		{
			const UWorld* World = ActiveInstance->GetWorld();
			if (World && !InstancesPerContext.Contains(World))
			{
				FText WorldName = FText::FromString(GetDebugStringForWorld(World));
				Contexts.Add(MakeShareable(new FObjectKey(World)));
				InstancesPerContext.Add(World, FFlowAssetInstanceContext(WorldName));
			}

			if (FFlowAssetInstanceContext* FoundContext = InstancesPerContext.Find(World))
			{
				FoundContext->AssetInstances.Add(Instances.Last());
			}
		}

		// todo: support Local Player context in case of split-screen
	}

	// set empty context by default, user must choose a specific context
	if (!SelectedContext.IsValid() || !Contexts.Contains(SelectedContext))
	{
		SelectedContext = NoContext;
	}

	// pre-select instance if current one does no longer exists
	if (!SelectedInstance.IsValid() || !Instances.Contains(SelectedInstance))
	{
		if (Instances.Num() > 1)
		{
			if (SelectedContext->ResolveObjectPtr())
			{
				// try to set first Instance for a selected context
				const FFlowAssetInstanceContext* InstanceContext = InstancesPerContext.Find(*SelectedContext.Get());
				if (InstanceContext && InstanceContext->AssetInstances.Num() > 0)
				{
					SelectedInstance = InstanceContext->AssetInstances[0];
				}
			}
			else
			{
				// set first active instance for any context
				SelectedInstance = Instances[1];
			}
		}
		else
		{
			// set empty instance if there's no active instances
			SelectedInstance = Instances[0];
		}
	}
}

EVisibility SFlowAssetInstanceList::GetContextVisibility() const
{
	// switching makes sense only if we have at least 1 specific context
	return InstancesPerContext.Num() > 1 ? EVisibility::Visible : EVisibility::Collapsed;
}

TSharedRef<SWidget> SFlowAssetInstanceList::OnGenerateContextWidget(TSharedPtr<FObjectKey> Item)
{
	const FFlowAssetInstanceContext* Context = InstancesPerContext.Find(Item->ResolveObjectPtr());
	return SNew(STextBlock).Text(Context ? Context->DisplayText : AllContextsText);
}

void SFlowAssetInstanceList::OnContextSelectionChanged(TSharedPtr<FObjectKey> SelectedItem, ESelectInfo::Type SelectionType)
{
	if (SelectionType != ESelectInfo::Direct)
	{
		SelectedContext = SelectedItem;

		if (TemplateAsset.IsValid())
		{
			TemplateAsset->SetInspectedInstance(nullptr);
		}
	}
}

FText SFlowAssetInstanceList::GetSelectedContextName() const
{
	if (SelectedContext.IsValid())
	{
		const UObject* Context = SelectedContext->ResolveObjectPtr();
		if (const FFlowAssetInstanceContext* InstanceContext = InstancesPerContext.Find(Context))
		{
			return InstanceContext->DisplayText;
		}
	}

	return AllContextsText;
}

TSharedRef<SWidget> SFlowAssetInstanceList::OnGenerateInstanceWidget(const TSharedPtr<FObjectKey> Item) const
{
	return SNew(STextBlock).Text(JoinInstanceAndContextTexts(*Item.Get()));
}

void SFlowAssetInstanceList::OnInstanceSelectionChanged(const TSharedPtr<FObjectKey> SelectedItem, const ESelectInfo::Type SelectionType)
{
	if (SelectionType != ESelectInfo::Direct)
	{
		SelectedInstance = SelectedItem;

		const UFlowAsset* Instance = Cast<UFlowAsset>(SelectedInstance->ResolveObjectPtr());
		if (TemplateAsset.IsValid())
		{
			TemplateAsset->SetInspectedInstance(Instance);
		}
	}
}

FText SFlowAssetInstanceList::GetSelectedInstanceName() const
{
	return SelectedInstance.IsValid() ? JoinInstanceAndContextTexts(*SelectedInstance.Get()) : NoInstanceSelectedText;
}

FText SFlowAssetInstanceList::JoinInstanceAndContextTexts(const FObjectKey& AssetInstance) const
{
	if (const UFlowAsset* Instance = Cast<UFlowAsset>(AssetInstance.ResolveObjectPtr()))
	{
		FText Result = FText::FromName(Instance->GetDisplayName());

		// add context name if there are multiple contexts present 
		if (InstancesPerContext.Num() > 1)
		{
			if (const FFlowAssetInstanceContext* Context = InstancesPerContext.Find(Instance->GetWorld()))
			{
				static const FText OpeningBracket = FText::AsCultureInvariant(TEXT("("));
				static const FText ClosingBracket = FText::AsCultureInvariant(TEXT(")"));
				Result = FText::Format(Result, OpeningBracket, Context->DisplayText, ClosingBracket);
			}
		}

		return Result;
	}

	return NoInstanceSelectedText;
}

//////////////////////////////////////////////////////////////////////////
// Flow Asset Breadcrumb

void SFlowAssetBreadcrumb::Construct(const FArguments& InArgs, TWeakObjectPtr<UFlowAsset> InTemplateAsset, TWeakPtr<FFlowAssetEditor> InAssetEditor)
{
	TemplateAsset = InTemplateAsset;
	AssetEditor = InAssetEditor;

	// Outer border visibility is the single source of truth; do not restrict the trail itself
	SAssignNew(BreadcrumbTrail, SBreadcrumbTrail<FFlowBreadcrumb>)
	.OnCrumbClicked(this, &SFlowAssetBreadcrumb::OnCrumbClicked)
	.ButtonStyle(FAppStyle::Get(), "SimpleButton")
	.TextStyle(FAppStyle::Get(), "NormalText")
	.ButtonContentPadding(FMargin(2.0f, 4.0f))
	.DelimiterImage(FAppStyle::GetBrush("Icons.ChevronRight"))
	.ShowLeadingDelimiter(true)
	.PersistentBreadcrumbs(true);

	ChildSlot
	[
		SNew(SBorder)
		.Visibility(this, &SFlowAssetBreadcrumb::GetBreadcrumbVisibility)
		.BorderImage(new FSlateRoundedBoxBrush(FStyleColors::Transparent, 4, FStyleColors::InputOutline, 1))
		[
			SNew(SBox)
			.MaxDesiredWidth(500.f)
			[
				BreadcrumbTrail.ToSharedRef()
			]
		]
	];

	// PIE refresh
	TemplateAsset->OnDebuggerRefresh().AddSP(this, &SFlowAssetBreadcrumb::FillBreadcrumb);
	// Edit-mode refresh: fired after EditNavParents is written by FlowGraphNode
	if (const TSharedPtr<FFlowAssetEditor> Ed = AssetEditor.Pin())
	{
		Ed->OnEditNavChanged.AddSP(this, &SFlowAssetBreadcrumb::FillBreadcrumb);
	}
	FillBreadcrumb();
}

EVisibility SFlowAssetBreadcrumb::GetBreadcrumbVisibility() const
{
	if (GEditor->PlayWorld)
	{
		return TemplateAsset->GetInspectedInstance() ? EVisibility::Visible : EVisibility::Collapsed;
	}

	// Edit mode: check editor nav chain first (fast path), then fall back to auto-discovery cache
	if (const TSharedPtr<FFlowAssetEditor> Ed = AssetEditor.Pin())
	{
		if (Ed->EditNavParents.Num() > 0)
		{
			return EVisibility::Visible;
		}
	}
	return bHasParentsForDisplay ? EVisibility::Visible : EVisibility::Collapsed;
}

// Returns all loaded FlowAssets that contain a SubGraph node pointing to ChildAsset.
// Uses FastGetAsset(false) — only inspects already-loaded objects to avoid synchronous loading.
static TArray<UFlowAsset*> FindDirectParents(const UFlowAsset* ChildAsset)
{
	TArray<UFlowAsset*> Result;
	if (!ChildAsset) return Result;

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	TArray<FName> ReferencerPackages;
	AssetRegistry.GetReferencers(ChildAsset->GetPackage()->GetFName(), ReferencerPackages,
	                              UE::AssetRegistry::EDependencyCategory::Package);

	for (const FName& PackageName : ReferencerPackages)
	{
		TArray<FAssetData> Assets;
		AssetRegistry.GetAssetsByPackageName(PackageName, Assets);

		for (const FAssetData& AssetData : Assets)
		{
			if (!AssetData.IsInstanceOf(UFlowAsset::StaticClass())) continue;

			// Only inspect already-loaded assets — avoids synchronous disk reads
			UFlowAsset* Candidate = Cast<UFlowAsset>(AssetData.FastGetAsset(false));
			if (!Candidate) continue;

			for (const auto& NodePair : Candidate->GetNodes())
			{
				if (const UFlowNode_SubGraph* Sub = Cast<UFlowNode_SubGraph>(NodePair.Value))
				{
					if (Sub->GetAssetToEdit() == ChildAsset)
					{
						Result.Add(Candidate);
						break;
					}
				}
			}
		}
	}
	return Result;
}

// Opens ParentAsset's editor and jumps to the SubGraph node that references ChildAsset.
static void NavigateToParent(UFlowAsset* ParentAsset, UFlowAsset* ChildAsset)
{
	if (!GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(ParentAsset)) return;
	const TSharedPtr<FFlowAssetEditor> Ed = FFlowGraphUtils::GetFlowAssetEditor(ParentAsset);
	if (!Ed || !ChildAsset) return;

	for (const auto& NodePair : ParentAsset->GetNodes())
	{
		if (UFlowNode_SubGraph* Sub = Cast<UFlowNode_SubGraph>(NodePair.Value))
		{
			if (Sub->GetAssetToEdit() == ChildAsset)
			{
				Ed->JumpToNode(Sub->GetGraphNode());
				break;
			}
		}
	}
}

void SFlowAssetBreadcrumb::BuildEditModeCrumbs(const TArray<TSoftObjectPtr<UFlowAsset>>& NavParents) const
{
	for (int32 i = 0; i < NavParents.Num(); i++)
	{
		// NavParents assets are already open (they were navigated through), so LoadSynchronous is safe
		UFlowAsset* Parent = NavParents[i].LoadSynchronous();
		if (!Parent) continue;
		UFlowAsset* Child = (i + 1 < NavParents.Num()) ? NavParents[i + 1].LoadSynchronous() : TemplateAsset.Get();
		BreadcrumbTrail->PushCrumb(FText::FromName(Parent->GetFName()), FFlowBreadcrumb(Parent, Child));
	}
	BreadcrumbTrail->PushCrumb(FText::FromName(TemplateAsset->GetFName()), FFlowBreadcrumb(TemplateAsset.Get(), nullptr));
	bHasParentsForDisplay = true;
}

void SFlowAssetBreadcrumb::FillBreadcrumb() const
{
	BreadcrumbTrail->ClearCrumbs();
	bHasParentsForDisplay = false;

	if (GEditor->PlayWorld)
	{
		// PIE: walk the runtime instance chain
		if (UFlowAsset* InspectedInstance = const_cast<UFlowAsset*>(TemplateAsset->GetInspectedInstance()))
		{
			TArray<TWeakObjectPtr<UFlowAsset>> InstancesFromRoot = {InspectedInstance};
			const UFlowAsset* CheckedInstance = InspectedInstance;
			while (UFlowAsset* ParentInstance = CheckedInstance->GetParentInstance())
			{
				InstancesFromRoot.Insert(ParentInstance, 0);
				CheckedInstance = ParentInstance;
			}
			for (int32 i = 0; i < InstancesFromRoot.Num(); i++)
			{
				TWeakObjectPtr<UFlowAsset> Child = i < InstancesFromRoot.Num() - 1 ? InstancesFromRoot[i + 1] : nullptr;
				BreadcrumbTrail->PushCrumb(FText::FromName(InstancesFromRoot[i]->GetDisplayName()),
				                           FFlowBreadcrumb(InstancesFromRoot[i], Child));
			}
		}
		return;
	}

	if (!TemplateAsset.IsValid()) return;

	// Edit mode path A: user navigated here via double-click — use the precise chain
	if (const TSharedPtr<FFlowAssetEditor> Ed = AssetEditor.Pin())
	{
		if (Ed->EditNavParents.Num() > 0)
		{
			BuildEditModeCrumbs(Ed->EditNavParents);
			return;
		}
	}

	// Edit mode path B: opened directly from Content Browser — auto-discover loaded parents
	TArray<UFlowAsset*> DiscoveredParents = FindDirectParents(TemplateAsset.Get());
	if (DiscoveredParents.Num() == 0) return;

	if (DiscoveredParents.Num() == 1)
	{
		TArray<TSoftObjectPtr<UFlowAsset>> SingleParent = {TSoftObjectPtr<UFlowAsset>(DiscoveredParents[0])};
		BuildEditModeCrumbs(SingleParent);
	}
	else
	{
		// Multiple parents: push a single indicator crumb with the full list attached
		FFlowBreadcrumb MultiCrumb;
		MultiCrumb.ChildInstance = TemplateAsset.Get();
		for (UFlowAsset* P : DiscoveredParents) MultiCrumb.AllParents.Add(P);

		BreadcrumbTrail->PushCrumb(
			FText::Format(LOCTEXT("MultiParentCrumb", "\u2191 {0} {0}|plural(one=reference,other=references) \u25be"),
			              FText::AsNumber(DiscoveredParents.Num())),
			MultiCrumb);
		BreadcrumbTrail->PushCrumb(FText::FromName(TemplateAsset->GetFName()), FFlowBreadcrumb(TemplateAsset.Get(), nullptr));
		bHasParentsForDisplay = true;
	}
}

void SFlowAssetBreadcrumb::OnCrumbClicked(const FFlowBreadcrumb& Item) const
{
	if (GEditor->PlayWorld)
	{
		// PIE: navigate to the clicked runtime instance
		const UFlowAsset* InspectedInstance = TemplateAsset->GetInspectedInstance();
		if (InspectedInstance == nullptr || Item.CurrentInstance != TemplateAsset)
		{
			UFlowAsset* ClickedInstance = Item.CurrentInstance.Get();
			UFlowAsset* ClickedTemplateAsset = ClickedInstance->GetTemplateAsset();
			if (GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(ClickedTemplateAsset))
			{
				ClickedTemplateAsset->SetInspectedInstance(ClickedInstance);
				if (const TSharedPtr<FFlowAssetEditor> FlowAssetEditor = FFlowGraphUtils::GetFlowAssetEditor(ClickedTemplateAsset))
				{
					if (Item.ChildInstance.IsValid())
					{
						FlowAssetEditor->JumpToNode(Item.ChildInstance->GetNodeOwningThisAssetInstance()->GetGraphNode());
					}
				}
			}
		}
		return;
	}

	// Edit mode — multi-parent indicator: show a dropdown so user picks which parent to visit
	if (Item.IsMultiParent())
	{
		UFlowAsset* ChildAsset = Item.ChildInstance.Get();

		FMenuBuilder MenuBuilder(true, nullptr);
		for (const TWeakObjectPtr<UFlowAsset>& ParentWeak : Item.AllParents)
		{
			UFlowAsset* ParentAsset = ParentWeak.Get();
			if (!ParentAsset) continue;

			MenuBuilder.AddMenuEntry(
				FText::FromName(ParentAsset->GetFName()),
				FText::GetEmpty(),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateLambda([ParentAsset, ChildAsset]()
				{
					NavigateToParent(ParentAsset, ChildAsset);
				})));
		}

		FSlateApplication::Get().PushMenu(
			AsShared(),
			FWidgetPath(),
			MenuBuilder.MakeWidget(),
			FSlateApplication::Get().GetCursorPos(),
			FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
		return;
	}

	// Edit mode — single ancestor crumb: open directly
	UFlowAsset* ClickedAsset = Item.CurrentInstance.Get();
	if (!ClickedAsset || ClickedAsset == TemplateAsset.Get()) return; // last crumb (current asset) is non-navigable
	NavigateToParent(ClickedAsset, Item.ChildInstance.Get());
}

//////////////////////////////////////////////////////////////////////////
// Flow Asset Toolbar

FFlowAssetToolbar::FFlowAssetToolbar(const TSharedPtr<FFlowAssetEditor> InAssetEditor, UToolMenu* ToolbarMenu)
	: FlowAssetEditor(InAssetEditor)
{
	BuildAssetToolbar(ToolbarMenu);
	BuildDebuggerToolbar(ToolbarMenu);
}

void FFlowAssetToolbar::BuildAssetToolbar(UToolMenu* ToolbarMenu) const
{
	{
		FToolMenuSection& Section = ToolbarMenu->AddSection("FlowAsset");
		Section.InsertPosition = FToolMenuInsert("Asset", EToolMenuInsertType::After);

		// add buttons
		Section.AddEntry(FToolMenuEntry::InitToolBarButton(FFlowToolbarCommands::Get().RefreshAsset));
		Section.AddEntry(FToolMenuEntry::InitToolBarButton(FFlowToolbarCommands::Get().ValidateAsset));
		Section.AddEntry(FToolMenuEntry::InitToolBarButton(FFlowToolbarCommands::Get().EditAssetDefaults));
	}

	{
		FToolMenuSection& Section = ToolbarMenu->AddSection("View");
		Section.InsertPosition = FToolMenuInsert("FlowAsset", EToolMenuInsertType::After);

		// Visual Diff: menu to choose asset revision compared with the current one 
		Section.AddDynamicEntry("SourceControlCommands", FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
		{
			const UFlowAssetEditorContext* Context = InSection.FindContext<UFlowAssetEditorContext>();
			if (Context && Context->FlowAssetEditor.IsValid())
			{
				InSection.InsertPosition = FToolMenuInsert();
				FToolMenuEntry DiffEntry = FToolMenuEntry::InitComboButton(
					"Diff",
					FUIAction(),
					FOnGetContent::CreateStatic(&FFlowAssetToolbar::MakeDiffMenu, Context),
					LOCTEXT("Diff", "Diff"),
					LOCTEXT("FlowAssetEditorDiffToolTip", "Diff against previous revisions"),
					FSlateIcon(FAppStyle::Get().GetStyleSetName(), "BlueprintDiff.ToolbarIcon")
				);
				DiffEntry.StyleNameOverride = "CalloutToolbar";
				InSection.AddEntry(DiffEntry);
			}
		}));

		Section.AddEntry(FToolMenuEntry::InitToolBarButton(
			FFlowToolbarCommands::Get().SearchInAsset,
			TAttribute<FText>(),
			TAttribute<FText>(),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Kismet.Tabs.FindResults")
		));
	}
}

/** Delegate called to diff a specific revision with the current */
// Copy from FBlueprintEditorToolbar::OnDiffRevisionPicked
static void OnDiffRevisionPicked(FRevisionInfo const& RevisionInfo, const FString& Filename, TWeakObjectPtr<UObject> CurrentAsset)
{
	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();

	// Get the SCC state
	const FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState(Filename, EStateCacheUsage::Use);
	if (SourceControlState.IsValid())
	{
		for (int32 HistoryIndex = 0; HistoryIndex < SourceControlState->GetHistorySize(); HistoryIndex++)
		{
			TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe> Revision = SourceControlState->GetHistoryItem(HistoryIndex);
			check(Revision.IsValid());
			if (Revision->GetRevision() == RevisionInfo.Revision)
			{
				// Get the revision of this package from source control
				FString PreviousTempPkgName;
				if (Revision->Get(PreviousTempPkgName))
				{
					// Try and load that package
					UPackage* PreviousTempPkg = LoadPackage(nullptr, *PreviousTempPkgName, LOAD_ForDiff | LOAD_DisableCompileOnLoad);
					if (PreviousTempPkg)
					{
						const FString PreviousAssetName = FPaths::GetBaseFilename(Filename, true);
						UObject* PreviousAsset = FindObject<UObject>(PreviousTempPkg, *PreviousAssetName);
						if (PreviousAsset)
						{
							const FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
							const FRevisionInfo OldRevision = {Revision->GetRevision(), Revision->GetCheckInIdentifier(), Revision->GetDate()};
							const FRevisionInfo CurrentRevision = {TEXT(""), Revision->GetCheckInIdentifier(), Revision->GetDate()};
							AssetToolsModule.Get().DiffAssets(PreviousAsset, CurrentAsset.Get(), OldRevision, CurrentRevision);
						}
					}
					else
					{
						FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("UnableToLoadAssets", "Unable to load assets to diff. Content may no longer be supported?"));
					}
				}
				break;
			}
		}
	}
}

// Variant of FBlueprintEditorToolbar::MakeDiffMenu
TSharedRef<SWidget> FFlowAssetToolbar::MakeDiffMenu(const UFlowAssetEditorContext* Context)
{
	if (ISourceControlModule::Get().IsEnabled() && ISourceControlModule::Get().GetProvider().IsAvailable())
	{
		UFlowAsset* FlowAsset = Context ? Context->FlowAssetEditor.Pin()->GetFlowAsset() : nullptr;
		if (FlowAsset)
		{
			FString Filename = SourceControlHelpers::PackageFilename(FlowAsset->GetPathName());
			TWeakObjectPtr<UObject> AssetPtr = FlowAsset;

			// Add our async SCC task widget
			return SNew(SAssetRevisionMenu, Filename)
				.OnRevisionSelected_Static(&OnDiffRevisionPicked, AssetPtr);
		}
		else
		{
			// if asset is null then this means that multiple assets are selected
			FMenuBuilder MenuBuilder(true, nullptr);
			MenuBuilder.AddMenuEntry(LOCTEXT("NoRevisionsForMultipleFlowAssets", "Multiple Flow Assets selected"), FText(), FSlateIcon(), FUIAction());
			return MenuBuilder.MakeWidget();
		}
	}

	FMenuBuilder MenuBuilder(true, nullptr);
	MenuBuilder.AddMenuEntry(LOCTEXT("SourceControlDisabled", "Source control is disabled"), FText(), FSlateIcon(), FUIAction());
	return MenuBuilder.MakeWidget();
}

void FFlowAssetToolbar::BuildDebuggerToolbar(UToolMenu* ToolbarMenu) const
{
	FToolMenuSection& Section = ToolbarMenu->AddSection("Debug");
	Section.InsertPosition = FToolMenuInsert("View", EToolMenuInsertType::After);

	Section.AddDynamicEntry("DebuggingCommands", FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
	{
		const UFlowAssetEditorContext* Context = InSection.FindContext<UFlowAssetEditorContext>();
		if (Context && Context->GetFlowAsset())
		{
			FPlayWorldCommands::BuildToolbar(InSection);

			InSection.AddEntry(FToolMenuEntry::InitWidget("AssetInstances", SNew(SFlowAssetInstanceList, Context->GetFlowAsset()), FText(), true));
			InSection.AddEntry(FToolMenuEntry::InitWidget("AssetBreadcrumb", SNew(SFlowAssetBreadcrumb, Context->GetFlowAsset(), Context->FlowAssetEditor), FText(), true));
		}
	}));
}

#undef LOCTEXT_NAMESPACE
