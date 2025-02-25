/*
 // Copyright (c) 2021-2022 Timothy Schoen
 // For information on usage and redistribution, and for a DISCLAIMER OF ALL
 // WARRANTIES, see the file, "LICENSE.txt," in this distribution.
*/

#pragma once

#include <utility>
#include "Utility/GlobalMouseListener.h"
#include "Utility/BouncingViewport.h"
#include "Constants.h"
#include "Canvas.h"
#include "PluginEditor.h"
#include "Utility/ZoomableDragAndDropContainer.h"

class WelcomePanel : public Component {

    class WelcomeButton : public Component {
        String iconText;
        String topText;
        String bottomText;

    public:
        std::function<void(void)> onClick = []() {};

        WelcomeButton(String icon, String mainText, String subText)
            : iconText(std::move(icon))
            , topText(std::move(mainText))
            , bottomText(std::move(subText))
        {
            setInterceptsMouseClicks(true, false);
            setAlwaysOnTop(true);
        }

        void paint(Graphics& g) override
        {
            auto colour = findColour(PlugDataColour::panelTextColourId);
            if (isMouseOver()) {
                g.setColour(findColour(PlugDataColour::panelActiveBackgroundColourId));
                PlugDataLook::fillSmoothedRectangle(g, Rectangle<float>(1, 1, getWidth() - 2, getHeight() - 2), Corners::largeCornerRadius);
                colour = findColour(PlugDataColour::panelActiveTextColourId);
            }

            Fonts::drawIcon(g, iconText, 20, 5, 40, colour, 24, false);
            Fonts::drawText(g, topText, 60, 7, getWidth() - 60, 20, colour, 16);
            Fonts::drawStyledText(g, bottomText, 60, 25, getWidth() - 60, 16, colour, Thin, 14);
        }

        void mouseUp(MouseEvent const& e) override
        {
            onClick();
        }

        void mouseEnter(MouseEvent const& e) override
        {
            repaint();
        }

        void mouseExit(MouseEvent const& e) override
        {
            repaint();
        }
    };

    class RecentlyOpenedListBox : public Component
        , public ListBoxModel {
    public:
        RecentlyOpenedListBox()
        {
            listBox.setRowHeight(26);
            listBox.setModel(this);
            listBox.setClickingTogglesRowSelection(true);
            update();

            listBox.setColour(ListBox::backgroundColourId, Colours::transparentBlack);

            addAndMakeVisible(listBox);

            bouncer = std::make_unique<BouncingViewportAttachment>(listBox.getViewport());
        }

        void update()
        {
            items.clear();

            auto settingsTree = SettingsFile::getInstance()->getValueTree();
            auto recentlyOpenedTree = settingsTree.getChildWithName("RecentlyOpened");
            if (recentlyOpenedTree.isValid()) {
                for (int i = 0; i < recentlyOpenedTree.getNumChildren(); i++) {
                    auto path = File(recentlyOpenedTree.getChild(i).getProperty("Path").toString());
                    items.add({ path.getFileName(), path });
                }
            }

            listBox.updateContent();
        }

        std::function<void(File)> onPatchOpen = [](File) {};

    private:
        int getNumRows() override
        {
            return items.size();
        }

        void listBoxItemClicked(int row, MouseEvent const& e) override
        {
            if (e.getNumberOfClicks() >= 2) {
                onPatchOpen(items[row].second);
            }
        }

        void paint(Graphics& g) override
        {
            g.setColour(findColour(PlugDataColour::outlineColourId));
            PlugDataLook::drawSmoothedRectangle(g, PathStrokeType(1.0f), Rectangle<float>(1, 32, getWidth() - 2, getHeight() - 32), Corners::defaultCornerRadius);

            Fonts::drawStyledText(g, "Recently Opened", 0, 0, getWidth(), 30, findColour(PlugDataColour::panelTextColourId), Semibold, 15, Justification::centred);
        }

        void resized() override
        {
            listBox.setBounds(getLocalBounds().withTrimmedTop(35));
        }

        void paintListBoxItem(int rowNumber, Graphics& g, int width, int height, bool rowIsSelected) override
        {
            /*
            if (rowIsSelected) {
                g.setColour(findColour(PlugDataColour::panelActiveBackgroundColourId));
                PlugDataLook::fillSmoothedRectangle(g, { 4.0f, 1.0f, width - 8.0f, height - 2.0f }, Corners::defaultCornerRadius);
            }

            auto colour = rowIsSelected ? findColour(PlugDataColour::panelActiveTextColourId) : findColour(PlugDataColour::panelTextColourId);

            Fonts::drawText(g, items[rowNumber].first, 12, 0, width - 9, height, colour, 15); */

            if (rowIsSelected) {
                g.setColour(findColour(PlugDataColour::panelActiveBackgroundColourId));
                PlugDataLook::fillSmoothedRectangle(g, Rectangle<float>(5.5, 1.5, width - 9, height - 4), Corners::defaultCornerRadius);
            }

            auto colour = rowIsSelected ? findColour(PlugDataColour::panelActiveTextColourId) : findColour(PlugDataColour::panelTextColourId);

            Fonts::drawText(g, items[rowNumber].first, height + 4, 0, width - 4, height, colour, 14);
            Fonts::drawIcon(g, Icons::File, 12, 0, height, colour, 12, false);
        }

        std::unique_ptr<BouncingViewportAttachment> bouncer;
        ListBox listBox;
        Array<std::pair<String, File>> items;
    };

public:
    WelcomePanel()
        : newButton(Icons::New, "New patch", "Create a new empty patch")
        , openButton(Icons::Open, "Open patch...", "Open a saved patch")

    {
        addAndMakeVisible(newButton);
        addAndMakeVisible(openButton);
        addAndMakeVisible(recentlyOpened);
    }

    void resized() override
    {
        newButton.setBounds(getLocalBounds().withSizeKeepingCentre(275, 50).translated(0, -70));
        openButton.setBounds(getLocalBounds().withSizeKeepingCentre(275, 50).translated(0, -10));

        if (getHeight() > 400) {
            recentlyOpened.setBounds(getLocalBounds().withSizeKeepingCentre(275, 170).translated(0, 110));
            recentlyOpened.setVisible(true);
        } else {
            recentlyOpened.setVisible(false);
        }
    }

    void show()
    {
        recentlyOpened.update();
        setVisible(true);
    }

    void hide()
    {
        setVisible(false);
    }

    void paint(Graphics& g) override
    {
        g.fillAll(findColour(PlugDataColour::panelBackgroundColourId));

        Fonts::drawStyledText(g, "No Patch Open", 0, getHeight() / 2 - 195, getWidth(), 40, findColour(PlugDataColour::panelTextColourId), Bold, 32, Justification::centred);

        Fonts::drawStyledText(g, "Open a file to begin patching", 0, getHeight() / 2 - 160, getWidth(), 40, findColour(PlugDataColour::panelTextColourId), Thin, 23, Justification::centred);
    }

    WelcomeButton newButton;
    WelcomeButton openButton;

    RecentlyOpenedListBox recentlyOpened;
};

class PluginEditor;

class ButtonBar : public TabbedButtonBar
    , public DragAndDropTarget
    , public ChangeListener {
public:
    ButtonBar(TabComponent& tabComp, TabbedButtonBar::Orientation o);

    bool isInterestedInDragSource(SourceDetails const& dragSourceDetails) override;
    void itemDropped(SourceDetails const& dragSourceDetails) override;
    void itemDragEnter(SourceDetails const& dragSourceDetails) override;
    void itemDragExit(SourceDetails const& dragSourceDetails) override;
    void itemDragMove(SourceDetails const& dragSourceDetails) override;

    void currentTabChanged(int newCurrentTabIndex, String const& newTabName) override;

    void changeListenerCallback(ChangeBroadcaster* source) override;

    TabBarButton* createTabButton(String const& tabName, int tabIndex) override;

    int getNumVisibleTabs();

    ComponentAnimator ghostTabAnimator;

private:
    TabComponent& owner;

    class GhostTab;
    std::unique_ptr<GhostTab> ghostTab;
    int ghostTabIdx = -1;
    bool inOtherSplit = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ButtonBar)
};

class TabComponent : public Component
    , public AsyncUpdater {

    TextButton newButton;
    WelcomePanel welcomePanel;
    PluginEditor* editor;

public:
    TabComponent(PluginEditor* editor);
    ~TabComponent();

    void onTabMoved();
    void onTabChange(int tabIndex);
    void newTab();
    void addTab(String const& tabName, Component* contentComponent, int insertIndex);
    void moveTab(int oldIndex, int newIndex);
    void clearTabs();
    void setTabBarDepth(int newDepth);
    Component* getTabContentComponent(int tabIndex) const noexcept;
    Component* getCurrentContentComponent() const noexcept { return panelComponent.get(); }
    int getCurrentTabIndex();
    void setCurrentTabIndex(int idx);
    int getNumTabs() const noexcept { return tabs->getNumTabs(); }
    int getNumVisibleTabs();
    void removeTab(int idx);
    int getTabBarDepth() const noexcept { return tabDepth; };
    void changeCallback(int newCurrentTabIndex, String const& newTabName);

    void openProject();
    void openProjectFile(File& patchFile);

    void currentTabChanged(int newCurrentTabIndex, String const& newCurrentTabName);
    void handleAsyncUpdate() override;
    void resized() override;

    void paint(Graphics& g) override;

    void paintOverChildren(Graphics& g) override;

    int getIndexOfCanvas(Canvas* cnv);

    void setTabText(int tabIndex, String const& newName);

    Canvas* getCanvas(int idx);

    Canvas* getCurrentCanvas();

    void setFocused();

    PluginEditor* getEditor();

    Image tabSnapshot;
    ScaledImage tabSnapshotScaled;

    Rectangle<int> tabSnapshotBounds;
    Rectangle<int> currentTabBounds;

private:
    int clickedTabIndex;
    int tabWidth;

    Point<int> scalePos;

    int draggedTabIndex = -1;
    Component* draggedTabComponent = nullptr;

    int tabDepth = 30;

    friend ButtonBar;

    Array<WeakReference<Component>> contentComponents;
    std::unique_ptr<TabbedButtonBar> tabs;
    WeakReference<Component> panelComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TabComponent)
};
