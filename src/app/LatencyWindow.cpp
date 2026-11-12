// LatencyWindow.cpp: see LatencyWindow.h.
#include "LatencyWindow.h"

#include <Button.h>
#include <Catalog.h>
#include <LayoutBuilder.h>
#include <String.h>
#include <StringView.h>
#include <TextControl.h>

#include <cstdlib>

#include "PowerMix.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "LatencyWindow"

enum { kMsgOK = 'okay' };

LatencyWindow::LatencyWindow(BWindow* parent, double outputDelay, int32 extra,
	BMessenger target)
	:
	BWindow(BRect(0, 0, 340, 120), B_TRANSLATE("Recording latency"), B_MODAL_WINDOW_LOOK,
		B_MODAL_SUBSET_WINDOW_FEEL, B_NOT_RESIZABLE | B_NOT_ZOOMABLE
			| B_AUTO_UPDATE_SIZE_LIMITS | B_CLOSE_ON_ESCAPE),
	fTarget(target)
{
	AddToSubset(parent);
	BString measured;
	measured.SetToFormat(B_TRANSLATE("Output delay (measured): %d ms"),
		int(outputDelay * 1000 + 0.5));
	BString value;
	value << extra;
	fExtra = new BTextControl(B_TRANSLATE("Extra input delay (ms):"), value.String(), NULL);
	BStringView* explain1 = new BStringView("explain1",
		B_TRANSLATE("Takes move earlier by both, to line up with the mix you heard."));
	BStringView* explain2 = new BStringView("explain2",
		B_TRANSLATE("If takes still come late, raise the extra delay; if early, lower it."));
	BButton* ok = new BButton(B_TRANSLATE("Set"), new BMessage(kMsgOK));
	BLayoutBuilder::Group<>(this, B_VERTICAL)
		.SetInsets(B_USE_WINDOW_SPACING)
		.Add(new BStringView("measured", measured.String()))
		.Add(fExtra)
		.Add(explain1)
		.Add(explain2)
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(new BButton(B_TRANSLATE("Cancel"), new BMessage(B_QUIT_REQUESTED)))
			.Add(ok)
		.End();
	SetDefaultButton(ok);
	fExtra->MakeFocus(true);
	CenterIn(parent->Frame());
}

void
LatencyWindow::MessageReceived(BMessage* message)
{
	if (message->what == kMsgOK) {
		BMessage reply(kMsgSetLatency);
		reply.AddInt32("ms", atoi(fExtra->Text()));
		fTarget.SendMessage(&reply);
		Quit();
		return;
	}
	BWindow::MessageReceived(message);
}
