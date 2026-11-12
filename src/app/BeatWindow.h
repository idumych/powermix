// BeatWindow.h: "Beat Info" (3dmiX's AskBeat): how many ticks a length
// holds, to set the Ticks field.
#ifndef POWERMIX_BEAT_WINDOW_H
#define POWERMIX_BEAT_WINDOW_H

#include <Messenger.h>
#include <Window.h>

class BStringView;
class BTextControl;

class BeatWindow : public BWindow {
public:
	// Replies kMsgSetTicks with "ticks" (float) to target
	BeatWindow(BWindow* parent, float length, BMessenger target);

	virtual void MessageReceived(BMessage* message);

private:
	void Update();

	float fLength;
	BMessenger fTarget;
	BTextControl* fCount;
	BStringView* fInfo;
};

#endif
