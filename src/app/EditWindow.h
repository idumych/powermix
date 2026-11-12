// EditWindow.h: the sound editor a double click on a clip opens (3dmiX's
// EditWindow and EditView): select part of the sound, cut, copy, paste,
// and filter it.
#ifndef POWERMIX_EDIT_WINDOW_H
#define POWERMIX_EDIT_WINDOW_H

#include <Messenger.h>
#include <Window.h>

#include <vector>

class AudioEngine;
class BMenuItem;
class BScrollBar;
class SoundView;

class EditWindow : public BWindow {
public:
	EditWindow(AudioEngine* engine, size_t track, size_t clip, BMessenger owner);

	virtual void MessageReceived(BMessage* message);
	virtual void MenusBeginning();

	bool Edits(size_t track, size_t clip) const { return track == fTrack && clip == fClip; }

private:
	friend class SoundView;

	void SaveUndo();
	void Apply();		// writes the sound back to the clip
	void AskSettings(uint32 filter, const char* name, bool delay, bool volume);

	AudioEngine* fEngine;
	size_t fTrack, fClip;
	BMessenger fOwner;
	std::vector<int16> fSound;
	std::vector<int16> fUndo;
	bool fHasUndo;
	SoundView* fView;
	BScrollBar* fScroll;
	BMenuItem* fUndoItem;
	BMenuItem* fPasteItem;
	float fValue1, fValue2;		// the dialogs' last values (EditView 0xf4, 0xf8)
};

#endif
