// BeatWindow.cpp: see BeatWindow.h. TrackView::DoBPM: ticks = 60 /
// (length / count).
#include "BeatWindow.h"

#include <Button.h>
#include <Catalog.h>
#include <LayoutBuilder.h>
#include <String.h>
#include <StringView.h>
#include <TextControl.h>

#include <cstdlib>

#include "PowerMix.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "BeatWindow"

enum {
	kMsgCountChanged = 'cnt!',
	kMsgOK = 'okay'
};

BeatWindow::BeatWindow(BWindow* parent, float length, BMessenger target)
	:
	BWindow(BRect(0, 0, 260, 100), B_TRANSLATE("Beat info"), B_MODAL_WINDOW_LOOK,
		B_MODAL_SUBSET_WINDOW_FEEL, B_NOT_RESIZABLE | B_NOT_ZOOMABLE
			| B_AUTO_UPDATE_SIZE_LIMITS | B_CLOSE_ON_ESCAPE),
	fLength(length),
	fTarget(target)
{
	AddToSubset(parent);
	fCount = new BTextControl(B_TRANSLATE("Number of ticks in selection:"), "4",
		new BMessage(kMsgCountChanged));
	fCount->SetModificationMessage(new BMessage(kMsgCountChanged));
	fInfo = new BStringView("info", "");
	BButton* ok = new BButton(B_TRANSLATE("Set ticks"), new BMessage(kMsgOK));
	BLayoutBuilder::Group<>(this, B_VERTICAL)
		.SetInsets(B_USE_WINDOW_SPACING)
		.Add(fCount)
		.Add(fInfo)
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(new BButton(B_TRANSLATE("Cancel"), new BMessage(B_QUIT_REQUESTED)))
			.Add(ok)
		.End();
	SetDefaultButton(ok);
	fCount->MakeFocus(true);
	CenterIn(parent->Frame());
	Update();
}

void
BeatWindow::Update()
{
	int count = atoi(fCount->Text());
	BString text;
	if (count > 0 && fLength > 0)
		text.SetToFormat(B_TRANSLATE("Ticks: %.3f"), 60.0 / (fLength / count));
	fInfo->SetText(text.String());
}

void
BeatWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgCountChanged:
			Update();
			break;
		case kMsgOK:
		{
			int count = atoi(fCount->Text());
			if (count > 0 && fLength > 0) {
				BMessage reply(kMsgSetTicks);
				reply.AddFloat("ticks", 60.0f / (fLength / count));
				fTarget.SendMessage(&reply);
			}
			Quit();
			break;
		}
		default:
			BWindow::MessageReceived(message);
	}
}
