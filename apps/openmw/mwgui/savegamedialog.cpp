#include "savegamedialog.hpp"
#include "widgets.hpp"

#include <OgreImage.h>
#include <OgreTextureManager.h>

#include <components/misc/stringops.hpp>

#include <components/settings/settings.hpp>

#include "../mwbase/statemanager.hpp"
#include "../mwbase/environment.hpp"
#include "../mwbase/world.hpp"
#include "../mwbase/windowmanager.hpp"

#include "../mwstate/character.hpp"

namespace MWGui
{
    SaveGameDialog::SaveGameDialog()
        : WindowModal("openmw_savegame_dialog.layout")
        , mSaving(true)
        , mCurrentCharacter(NULL)
    {
        getWidget(mScreenshot, "Screenshot");
        getWidget(mCharacterSelection, "SelectCharacter");
        getWidget(mInfoText, "InfoText");
        getWidget(mOkButton, "OkButton");
        getWidget(mCancelButton, "CancelButton");
        getWidget(mSaveList, "SaveList");
        getWidget(mSaveNameEdit, "SaveNameEdit");
        getWidget(mSpacer, "Spacer");
        mOkButton->eventMouseButtonClick += MyGUI::newDelegate(this, &SaveGameDialog::onOkButtonClicked);
        mCancelButton->eventMouseButtonClick += MyGUI::newDelegate(this, &SaveGameDialog::onCancelButtonClicked);
        mCharacterSelection->eventComboChangePosition += MyGUI::newDelegate(this, &SaveGameDialog::onCharacterSelected);
        mSaveList->eventListChangePosition += MyGUI::newDelegate(this, &SaveGameDialog::onSlotSelected);

    }

    void SaveGameDialog::open()
    {
        WindowModal::open();

        mSaveNameEdit->setCaption ("");

        center();

        MWBase::StateManager* mgr = MWBase::Environment::get().getStateManager();
        if (mgr->characterBegin() == mgr->characterEnd())
            return;

        mCurrentCharacter = mgr->getCurrentCharacter (false);

        std::string directory =
            Misc::StringUtils::lowerCase (Settings::Manager::getString ("character", "Saves"));

        mCharacterSelection->removeAllItems();

        for (MWBase::StateManager::CharacterIterator it = mgr->characterBegin(); it != mgr->characterEnd(); ++it)
        {
            if (it->begin()!=it->end())
            {
                std::stringstream title;
                title << it->getSignature().mPlayerName;
                title << " (Level " << it->getSignature().mPlayerLevel << " " << it->getSignature().mPlayerClass << ")";

                mCharacterSelection->addItem (title.str());

                if (mCurrentCharacter == &*it ||
                    (!mCurrentCharacter && !mSaving && directory==Misc::StringUtils::lowerCase (
                    it->begin()->mPath.parent_path().filename().string())))
                {
                    mCurrentCharacter = &*it;
                    mCharacterSelection->setIndexSelected(mCharacterSelection->getItemCount()-1);
                }
            }
        }

        fillSaveList();

    }

    void SaveGameDialog::setLoadOrSave(bool load)
    {
        mSaving = !load;
        mSaveNameEdit->setVisible(!load);
        mCharacterSelection->setUserString("Hidden", load ? "false" : "true");
        mCharacterSelection->setVisible(load);
        mSpacer->setUserString("Hidden", load ? "false" : "true");

        if (!load)
        {
            mCurrentCharacter = MWBase::Environment::get().getStateManager()->getCurrentCharacter (false);
        }

        center();
    }

    void SaveGameDialog::onCancelButtonClicked(MyGUI::Widget *sender)
    {
        setVisible(false);
    }

    void SaveGameDialog::onOkButtonClicked(MyGUI::Widget *sender)
    {
        // Get the selected slot, if any
        unsigned int i=0;
        const MWState::Slot* slot = NULL;

        if (mCurrentCharacter)
        {
            for (MWState::Character::SlotIterator it = mCurrentCharacter->begin(); it != mCurrentCharacter->end(); ++it,++i)
            {
                if (i == mSaveList->getIndexSelected())
                    slot = &*it;
            }
        }

        if (mSaving)
        {
            MWBase::Environment::get().getStateManager()->saveGame (mSaveNameEdit->getCaption(), slot);
        }
        else
        {
            if (mCurrentCharacter && slot)
                MWBase::Environment::get().getStateManager()->loadGame (mCurrentCharacter, slot);
        }

        setVisible(false);

        if (MWBase::Environment::get().getStateManager()->getState()==
            MWBase::StateManager::State_NoGame)
        {
            MWBase::Environment::get().getWindowManager()->pushGuiMode (MWGui::GM_MainMenu);
        }
    }

    void SaveGameDialog::onCharacterSelected(MyGUI::ComboBox *sender, size_t pos)
    {
        MWBase::StateManager* mgr = MWBase::Environment::get().getStateManager();

        unsigned int i=0;
        const MWState::Character* character = NULL;
        for (MWBase::StateManager::CharacterIterator it = mgr->characterBegin(); it != mgr->characterEnd(); ++it, ++i)
        {
            if (i == pos)
                character = &*it;
        }
        assert(character && "Can't find selected character");

        mCurrentCharacter = character;
        fillSaveList();
    }

    void SaveGameDialog::fillSaveList()
    {
        mSaveList->removeAllItems();
        if (!mCurrentCharacter)
            return;
        for (MWState::Character::SlotIterator it = mCurrentCharacter->begin(); it != mCurrentCharacter->end(); ++it)
        {
            mSaveList->addItem(it->mProfile.mDescription);
        }
        onSlotSelected(mSaveList, MyGUI::ITEM_NONE);
    }

    void SaveGameDialog::onSlotSelected(MyGUI::ListBox *sender, size_t pos)
    {
        if (pos == MyGUI::ITEM_NONE)
        {
            mInfoText->setCaption("");
            mScreenshot->setImageTexture("");
            return;
        }

        const MWState::Slot* slot = NULL;
        unsigned int i=0;
        for (MWState::Character::SlotIterator it = mCurrentCharacter->begin(); it != mCurrentCharacter->end(); ++it, ++i)
        {
            if (i == pos)
                slot = &*it;
        }
        assert(slot && "Can't find selected slot");

        std::stringstream text;
        time_t time = slot->mTimeStamp;
        struct tm* timeinfo;
        timeinfo = localtime(&time);

        text << asctime(timeinfo) << "\n";
        text << "Level " << slot->mProfile.mPlayerLevel << "\n";
        text << slot->mProfile.mPlayerCell << "\n";
        // text << "Time played: " << slot->mProfile.mTimePlayed << "\n";

        int hour = int(slot->mProfile.mInGameTime.mGameHour);
        bool pm = hour >= 12;
        if (hour >= 13) hour -= 12;
        if (hour == 0) hour = 12;

        text
            << slot->mProfile.mInGameTime.mDay << " "
            << MWBase::Environment::get().getWorld()->getMonthName(slot->mProfile.mInGameTime.mMonth)
            <<  " " << hour << " " << (pm ? "#{sSaveMenuHelp05}" : "#{sSaveMenuHelp04}");

        mInfoText->setCaptionWithReplacing(text.str());

        // Decode screenshot
        std::vector<char> data = slot->mProfile.mScreenshot; // MemoryDataStream doesn't work with const data :(
        Ogre::DataStreamPtr stream(new Ogre::MemoryDataStream(&data[0], data.size()));
        Ogre::Image image;
        image.load(stream, "jpg");

        const std::string textureName = "@savegame_screenshot";
        Ogre::TexturePtr texture;
        texture = Ogre::TextureManager::getSingleton().getByName(textureName);
        mScreenshot->setImageTexture("");
        if (texture.isNull())
        {
            texture = Ogre::TextureManager::getSingleton().createManual(textureName,
                Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
                Ogre::TEX_TYPE_2D,
                image.getWidth(), image.getHeight(), 0, Ogre::PF_BYTE_RGBA, Ogre::TU_DYNAMIC_WRITE_ONLY);
        }
        texture->unload();
        texture->setWidth(image.getWidth());
        texture->setHeight(image.getHeight());
        texture->loadImage(image);

        mScreenshot->setImageTexture(textureName);
    }
}
