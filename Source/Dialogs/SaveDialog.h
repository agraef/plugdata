/*
 // Copyright (c) 2021-2022 Timothy Schoen
 // For information on usage and redistribution, and for a DISCLAIMER OF ALL
 // WARRANTIES, see the file, "LICENSE.txt," in this distribution.
 */

class SaveDialogButton : public TextButton {
public:
    SaveDialogButton(String buttonText)
        : TextButton(buttonText)
    {
    }

private:
    void paint(Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(1.0f);

        auto backgroundColour = findColour(PlugDataColour::dialogBackgroundColourId);
        auto activeColour = findColour(PlugDataColour::toolbarActiveColourId);

        if (isMouseOver() || isMouseButtonDown()) {
            backgroundColour = backgroundColour.contrasting(0.05f);
        }

        g.setColour(backgroundColour);
        PlugDataLook::fillSmoothedRectangle(g, bounds, Corners::defaultCornerRadius);

        g.setFont(Fonts::getDefaultFont().withHeight(15));
        g.setColour(findColour(PlugDataColour::panelTextColourId));

        g.drawText(getButtonText(), getLocalBounds().reduced(3), Justification::centred);

        auto outlineColour = hasKeyboardFocus(false) ? activeColour : findColour(PlugDataColour::outlineColourId);

        g.setColour(outlineColour);
        PlugDataLook::drawSmoothedRectangle(g, PathStrokeType(1.0f), bounds, Corners::defaultCornerRadius);
    }
};

class SaveDialog : public Component {

public:
    SaveDialog(Dialog* parent, String const& filename, std::function<void(int)> callback, bool withLogo)
        : savelabel("savelabel", filename.isEmpty() ? "Save changes before closing?" : "Save changes to \"" + filename + "\"\n before closing?"), hasLogo(withLogo)
    {
        cb = callback;
        setSize(265, 270);
        addAndMakeVisible(savelabel);
        addAndMakeVisible(cancel);
        addAndMakeVisible(dontsave);
        addAndMakeVisible(save);

        savelabel.setFont(Fonts::getBoldFont().withHeight(15.0f));
        savelabel.setJustificationType(Justification::centred);

        cancel.onClick = [parent] {
            MessageManager::callAsync(
                [parent]() {
                    cb(0);
                    parent->closeDialog();
                });
        };
        save.onClick = [parent] {
            MessageManager::callAsync(
                [parent]() {
                    cb(2);
                    parent->closeDialog();
                });
        };
        dontsave.onClick = [parent] {
            MessageManager::callAsync(
                [parent]() {
                    cb(1);
                    parent->closeDialog();
                });
        };

        cancel.setColour(TextButton::buttonColourId, Colours::transparentBlack);
        dontsave.setColour(TextButton::buttonColourId, Colours::transparentBlack);
        save.setColour(TextButton::buttonColourId, Colours::transparentBlack);

        cancel.setColour(TextButton::textColourOnId, findColour(TextButton::textColourOffId));
        dontsave.setColour(TextButton::textColourOnId, findColour(TextButton::textColourOffId));
        save.setColour(TextButton::textColourOnId, findColour(TextButton::textColourOffId));

        setOpaque(false);

        MessageManager::callAsync([_this = SafePointer(this)]() {
            if (_this) {
                // Move window to front when opening dialog
                if(auto* topLevel = _this->getTopLevelComponent()) topLevel->toFront(false);
    
                _this->save.grabKeyboardFocus();
            }
        });
    }

    void paint(Graphics& g) override
    {
        if(!hasLogo) return;
        
        auto contentBounds = getLocalBounds().reduced(16);
        auto logoBounds = contentBounds.removeFromTop(contentBounds.getHeight() / 3.5f).withSizeKeepingCentre(64, 64);

        g.setImageResamplingQuality(Graphics::highResamplingQuality);
        g.drawImage(logo, logoBounds.toFloat());
        g.setImageResamplingQuality(Graphics::mediumResamplingQuality);
    }

    void resized() override
    {
        auto contentBounds = getLocalBounds().reduced(16);

        // logo space
        if(hasLogo) {
            contentBounds.removeFromTop(contentBounds.getHeight() / 3.5f + 8.0f);
        }
    
        savelabel.setBounds(contentBounds.removeFromTop(contentBounds.getHeight() / 3));
        contentBounds.removeFromTop(8);

        save.setBounds(contentBounds.removeFromTop(26));
        contentBounds.removeFromTop(6);
        dontsave.setBounds(contentBounds.removeFromTop(26));
        contentBounds.removeFromTop(16);
        cancel.setBounds(contentBounds.removeFromTop(26));
    }

    static inline std::function<void(int)> cb = [](int) {};

private:
    bool hasLogo;
    Label savelabel;

    Image logo = ImageFileFormat::loadFrom(BinaryData::plugdata_large_logo_png, BinaryData::plugdata_large_logo_pngSize);

    SaveDialogButton cancel = SaveDialogButton("Cancel");
    SaveDialogButton dontsave = SaveDialogButton("Don't Save");
    SaveDialogButton save = SaveDialogButton("Save");
};
