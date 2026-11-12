// LatencyWindow.h: Misc ▸ Recording latency…: how much earlier takes are
// moved so they line up with the mix that was heard while recording.
#ifndef POWERMIX_LATENCY_WINDOW_H
#define POWERMIX_LATENCY_WINDOW_H

#include <Messenger.h>
#include <Window.h>

class BTextControl;

class LatencyWindow : public BWindow {
public:
	// outputDelay: measured, seconds; extra: the setting, milliseconds
	LatencyWindow(BWindow* parent, double outputDelay, int32 extra, BMessenger target);

	virtual void MessageReceived(BMessage* message);

private:
	BTextControl* fExtra;
	BMessenger fTarget;
};

#endif
