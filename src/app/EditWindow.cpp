// EditWindow.cpp: see EditWindow.h. The filters are in src/core/Effects.
#include "EditWindow.h"

#include <Button.h>
#include <Catalog.h>
#include <LayoutBuilder.h>
#include <Menu.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <ScrollBar.h>
#include <Slider.h>
#include <String.h>

#include <algorithm>
#include <cmath>

#include "AudioEngine.h"
#include "Effects.h"
#include "Palette.h"
#include "PowerMix.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "EditWindow"

namespace {

enum {
	// EditWindow's codes
	kMsgZero = 0x400,
	kMsgVolume = 0x401,
	kMsgMaxVolume = 0x402,
	kMsgEcho = 0x404,
	kMsgChorus = 0x405,
	kMsgMetal = 0x406,
	kMsgFade = 0x407,
	kMsgCompress = 0x408,
	kMsgSettings = 'Sett',
	kMsgScroll = 'Scrl'
};

// The sound editor's own clipboard (EditView's do_copy)
std::vector<int16> sClipboard;

}	// namespace

// #pragma mark - SoundView

// EditView: the sound in blue on grey, the selection inverted, a ruler on top
class SoundView : public BView {
public:
	SoundView(BRect frame, EditWindow* window)
		:
		BView(frame, "edit view", B_FOLLOW_ALL_SIDES, B_WILL_DRAW | B_FRAME_EVENTS),
		fWindow(window),
		fFramesPerPixel(64),
		fStart(0),
		fSelFrom(0),
		fSelTo(0),
		fTracking(false)
	{
		SetViewColor(B_TRANSPARENT_COLOR);
	}

	static const int kRuler = 18;

	size_t FrameAt(float x) const
	{
		double f = fStart + double(x) * fFramesPerPixel;
		return size_t(std::max(0.0, std::min(f, double(fWindow->fSound.size()))));
	}

	float XAt(size_t frame) const { return float((double(frame) - fStart) / fFramesPerPixel); }

	virtual void Draw(BRect updateRect)
	{
		const std::vector<int16>& s = fWindow->fSound;
		BRect b = Bounds();
		// ruler
		SetHighColor(64, 64, 64);
		FillRect(BRect(b.left, 0, b.right, kRuler - 1));
		BFont font;
		GetFont(&font);
		font.SetSize(9);
		SetFont(&font);
		double step = 0.01;
		while (step * pmx::kSampleRate / fFramesPerPixel < 70)
			step *= 2;
		SetLowColor(64, 64, 64);
		for (double t = floor(fStart / pmx::kSampleRate / step) * step;
				XAt(size_t(t * pmx::kSampleRate)) < b.right; t += step) {
			float x = XAt(size_t(t * pmx::kSampleRate));
			SetHighColor(200, 200, 200);
			StrokeLine(BPoint(x, kRuler - 6), BPoint(x, kRuler - 1));
			BString label;
			label.SetToFormat("%.2f", t);
			SetHighColor(230, 230, 230);
			DrawString(label.String(), BPoint(x + 2, 10));
		}
		// the sound
		BRect wave(b.left, kRuler, b.right, b.bottom);
		SetHighColor(200, 200, 200);
		FillRect(wave);
		const float mid = (wave.top + wave.bottom) / 2;
		const float scale = (wave.Height() / 2 - 2) / 32768.0f;
		SetHighColor(0, 0, 255);
		for (float x = std::max(b.left, updateRect.left); x <= std::min(b.right, updateRect.right);
				x++) {
			size_t from = FrameAt(x), to = std::max(from + 1, FrameAt(x + 1));
			if (from >= s.size())
				break;
			int lo = 0, hi = 0;
			for (size_t k = from; k < to && k < s.size(); k++) {
				lo = std::min<int>(lo, s[k]);
				hi = std::max<int>(hi, s[k]);
			}
			StrokeLine(BPoint(x, mid - hi * scale), BPoint(x, mid - lo * scale));
		}
		SetHighColor(120, 120, 120);
		StrokeLine(BPoint(b.left, mid), BPoint(b.right, mid));
		// selection
		if (fSelTo > fSelFrom) {
			SetDrawingMode(B_OP_INVERT);
			FillRect(BRect(XAt(fSelFrom), wave.top, XAt(fSelTo), wave.bottom));
			SetDrawingMode(B_OP_COPY);
		}
	}

	// EditView::MouseDown: drag to select, Shift extends
	virtual void MouseDown(BPoint where)
	{
		SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS);
		uint32 modifiers = 0;
		Window()->CurrentMessage()->FindInt32("modifiers", (int32*)&modifiers);
		size_t f = FrameAt(where.x);
		if ((modifiers & B_SHIFT_KEY) != 0 && fSelTo > fSelFrom) {
			fAnchor = (f < (fSelFrom + fSelTo) / 2) ? fSelTo : fSelFrom;
		} else
			fAnchor = f;
		fTracking = true;
		MouseMoved(where, 0, NULL);
	}

	virtual void MouseMoved(BPoint where, uint32 transit, const BMessage* drag)
	{
		if (!fTracking)
			return;
		size_t f = FrameAt(where.x);
		fSelFrom = std::min(fAnchor, f);
		fSelTo = std::max(fAnchor, f);
		Invalidate();
	}

	virtual void MouseUp(BPoint where)
	{
		fTracking = false;
	}

	virtual void KeyDown(const char* bytes, int32 count)
	{
		if (bytes[0] == '+' || bytes[0] == '=')
			SetZoom(fFramesPerPixel / 1.5);
		else if (bytes[0] == '-')
			SetZoom(fFramesPerPixel * 1.5);
		else
			BView::KeyDown(bytes, count);
	}

	virtual void FrameResized(float width, float height)
	{
		UpdateScroll();
	}

	void SelectAll()
	{
		fSelFrom = 0;
		fSelTo = fWindow->fSound.size();
		Invalidate();
	}

	// the selection, or the whole sound if nothing is selected
	void Range(size_t& from, size_t& to) const
	{
		if (fSelTo > fSelFrom) {
			from = fSelFrom;
			to = std::min(fSelTo, fWindow->fSound.size());
		} else {
			from = 0;
			to = fWindow->fSound.size();
		}
	}

	void SetZoom(double framesPerPixel)
	{
		fFramesPerPixel = std::max(1.0 / 8, framesPerPixel);
		UpdateScroll();
		Invalidate();
	}

	double Zoom() const { return fFramesPerPixel; }

	void SetStart(double frame)
	{
		fStart = frame;
		Invalidate();
	}

	void UpdateScroll()
	{
		BScrollBar* bar = fWindow->fScroll;
		if (bar == NULL)
			return;
		float visible = Bounds().Width() * float(fFramesPerPixel);
		float range = std::max(0.0f, float(fWindow->fSound.size()) - visible);
		bar->SetRange(0, range);
		bar->SetProportion(fWindow->fSound.empty() ? 1
			: std::min(1.0f, visible / float(fWindow->fSound.size())));
		bar->SetSteps(float(fFramesPerPixel) * 8, visible / 2);
	}

	size_t fSelFrom, fSelTo;

private:
	EditWindow* fWindow;
	double fFramesPerPixel;
	double fStart;
	size_t fAnchor = 0;
	bool fTracking;
};

class FrameScrollBar : public BScrollBar {
public:
	FrameScrollBar(BRect frame, SoundView* view)
		:
		BScrollBar(frame, "_HSB_", NULL, 0, 0, B_HORIZONTAL),
		fView(view)
	{
		SetResizingMode(B_FOLLOW_LEFT_RIGHT | B_FOLLOW_BOTTOM);
	}

	virtual void ValueChanged(float value)
	{
		BScrollBar::ValueChanged(value);
		fView->SetStart(value);
	}

private:
	SoundView* fView;
};

// #pragma mark - Settings

// AskVolume, AskStrength, AskFilter: "<Filter> settings" with one or two
// sliders; replies kMsgSettings with "filter", "value1", "value2"
class SettingsWindow : public BWindow {
public:
	SettingsWindow(BWindow* parent, uint32 filter, const char* name, bool delay, bool volume,
		float value1, float value2)
		:
		BWindow(BRect(0, 0, 300, 100), "", B_MODAL_WINDOW_LOOK, B_MODAL_SUBSET_WINDOW_FEEL,
			B_NOT_RESIZABLE | B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS | B_CLOSE_ON_ESCAPE),
		fParent(parent),
		fFilter(filter),
		fDelay(NULL)
	{
		BString title;
		title.SetToFormat(B_TRANSLATE("%s settings"), name);
		SetTitle(title.String());
		AddToSubset(parent);
		if (volume) {
			fValue = new BSlider("value", B_TRANSLATE("Volume change"), NULL, 0, 200,
				B_HORIZONTAL);
			fValue->SetLimitLabels("0.0%", "200.0%");
		} else {
			fValue = new BSlider("value", B_TRANSLATE("Strength"), NULL, 0, 100, B_HORIZONTAL);
			fValue->SetLimitLabels("0.0%", "100.0%");
		}
		fValue->SetValue(int32(value1));
		fValue->SetHashMarks(B_HASH_MARKS_BOTTOM);
		fValue->SetHashMarkCount(11);
		BButton* ok = new BButton(B_TRANSLATE("Apply"), new BMessage('okay'));
		BLayoutBuilder::Group<> builder(this, B_VERTICAL);
		builder.SetInsets(B_USE_WINDOW_SPACING).Add(fValue);
		if (delay) {
			fDelay = new BSlider("delay", B_TRANSLATE("Delay"), NULL, 1, 1000, B_HORIZONTAL);
			fDelay->SetLimitLabels("1 ms", "1000 ms");
			fDelay->SetValue(int32(std::max(1.0f, value2)));
			builder.Add(fDelay);
		}
		builder.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(new BButton(B_TRANSLATE("Cancel"), new BMessage(B_QUIT_REQUESTED)))
			.Add(ok)
		.End();
		SetDefaultButton(ok);
		CenterIn(parent->Frame());
	}

	virtual void MessageReceived(BMessage* message)
	{
		if (message->what == 'okay') {
			BMessage reply(kMsgSettings);
			reply.AddInt32("filter", int32(fFilter));
			reply.AddFloat("value1", float(fValue->Value()));
			reply.AddFloat("value2", fDelay != NULL ? float(fDelay->Value()) : 0);
			fParent->PostMessage(&reply);
			Quit();
			return;
		}
		BWindow::MessageReceived(message);
	}

private:
	BWindow* fParent;
	uint32 fFilter;
	BSlider* fValue;
	BSlider* fDelay;
};

// #pragma mark - EditWindow

EditWindow::EditWindow(AudioEngine* engine, size_t track, size_t clip, BMessenger owner)
	:
	BWindow(BRect(200, 200, 800, 500), "", B_DOCUMENT_WINDOW, B_AUTO_UPDATE_SIZE_LIMITS),
	fEngine(engine),
	fTrack(track),
	fClip(clip),
	fOwner(owner),
	fHasUndo(false),
	fScroll(NULL),
	fValue1(100),
	fValue2(100)
{
	BString title("PowerMix");
	if (fEngine->Lock()) {
		const pmx::Project& project = fEngine->Project();
		if (track < project.tracks.size() && clip < project.tracks[track].clips.size()) {
			const pmx::Clip& c = project.tracks[track].clips[clip];
			fSound = c.samples;
			title = c.source.empty() ? project.tracks[track].name.c_str() : c.source.c_str();
		}
		fEngine->Unlock();
	}
	SetTitle(title.String());

	BMenuBar* bar = new BMenuBar(Bounds(), "menu");
	BMenu* menu = new BMenu(B_TRANSLATE("Edit"));
	menu->AddItem(fUndoItem = new BMenuItem(B_TRANSLATE("Undo"), new BMessage(kMsgUndo), 'Z'));
	menu->AddSeparatorItem();
	menu->AddItem(new BMenuItem(B_TRANSLATE("Clear"), new BMessage(kMsgClear)));
	menu->AddItem(new BMenuItem(B_TRANSLATE("Cut"), new BMessage(kMsgCut), 'X'));
	menu->AddItem(new BMenuItem(B_TRANSLATE("Copy"), new BMessage(kMsgCopy), 'C'));
	menu->AddItem(fPasteItem = new BMenuItem(B_TRANSLATE("Paste"), new BMessage(kMsgPaste),
		'V'));
	menu->AddSeparatorItem();
	menu->AddItem(new BMenuItem(B_TRANSLATE("Select all"), new BMessage(B_SELECT_ALL), 'A'));
	bar->AddItem(menu);
	menu = new BMenu(B_TRANSLATE("Filter"));
	menu->AddItem(new BMenuItem(B_TRANSLATE("Zero"), new BMessage(kMsgZero)));
	menu->AddItem(new BMenuItem(B_TRANSLATE("Change volume" B_UTF8_ELLIPSIS),
		new BMessage(kMsgVolume)));
	menu->AddItem(new BMenuItem(B_TRANSLATE("Max volume"), new BMessage(kMsgMaxVolume)));
	menu->AddItem(new BMenuItem(B_TRANSLATE("Echo" B_UTF8_ELLIPSIS), new BMessage(kMsgEcho)));
	menu->AddItem(new BMenuItem(B_TRANSLATE("Chorus" B_UTF8_ELLIPSIS), new BMessage(kMsgChorus)));
	menu->AddItem(new BMenuItem(B_TRANSLATE("Metal"), new BMessage(kMsgMetal)));
	menu->AddItem(new BMenuItem(B_TRANSLATE("Fade" B_UTF8_ELLIPSIS), new BMessage(kMsgFade)));
	menu->AddItem(new BMenuItem(B_TRANSLATE("Compress" B_UTF8_ELLIPSIS),
		new BMessage(kMsgCompress)));
	bar->AddItem(menu);
	AddChild(bar);

	BRect frame = Bounds();
	frame.top = bar->Frame().bottom + 1;
	frame.bottom -= B_H_SCROLL_BAR_HEIGHT + 1;
	fView = new SoundView(frame, this);
	AddChild(fView);
	BRect scroll = Bounds();
	scroll.top = scroll.bottom - B_H_SCROLL_BAR_HEIGHT;
	scroll.right -= B_V_SCROLL_BAR_WIDTH;
	fScroll = new FrameScrollBar(scroll, fView);
	AddChild(fScroll);
	// the whole sound fits at first
	fView->SetZoom(std::max(1.0, double(fSound.size()) / std::max(1.0f, frame.Width())));
	fView->MakeFocus(true);
}

void
EditWindow::MenusBeginning()
{
	fUndoItem->SetEnabled(fHasUndo);
	fPasteItem->SetEnabled(!sClipboard.empty());
}

// EditView::SaveUndo
void
EditWindow::SaveUndo()
{
	fUndo = fSound;
	fHasUndo = true;
}

void
EditWindow::Apply()
{
	if (fEngine->Lock()) {
		pmx::Project& project = fEngine->Project();
		if (fTrack < project.tracks.size() && fClip < project.tracks[fTrack].clips.size())
			project.tracks[fTrack].clips[fClip].samples = fSound;
		fEngine->Unlock();
	}
	fView->UpdateScroll();
	fView->Invalidate();
	fOwner.SendMessage(kMsgClipsChanged);	// main_refresh
}

void
EditWindow::AskSettings(uint32 filter, const char* name, bool delay, bool volume)
{
	(new SettingsWindow(this, filter, name, delay, volume, fValue1, fValue2))->Show();
}

void
EditWindow::MessageReceived(BMessage* message)
{
	size_t from, to;
	fView->Range(from, to);
	switch (message->what) {
		case kMsgUndo:
			if (fHasUndo) {
				std::swap(fUndo, fSound);
				Apply();
			}
			break;
		case kMsgCopy:
			if (fView->fSelTo > fView->fSelFrom)
				pmx::CopyFrames(fSound, from, to, sClipboard);
			break;
		case kMsgCut:
		case kMsgClear:
			if (fView->fSelTo > fView->fSelFrom) {
				SaveUndo();
				if (message->what == kMsgCut)
					pmx::CopyFrames(fSound, from, to, sClipboard);
				pmx::DeleteFrames(fSound, from, to);
				fView->fSelTo = fView->fSelFrom;
				Apply();
			}
			break;
		case kMsgPaste:
			if (!sClipboard.empty()) {
				SaveUndo();
				if (fView->fSelTo > fView->fSelFrom)
					pmx::DeleteFrames(fSound, from, to);
				pmx::InsertFrames(fSound, fView->fSelFrom, sClipboard);
				fView->fSelTo = fView->fSelFrom + sClipboard.size();
				Apply();
			}
			break;
		case B_SELECT_ALL:
			fView->SelectAll();
			break;

		case kMsgZero:
			SaveUndo();
			pmx::Zero(fSound, from, to);
			Apply();
			break;
		case kMsgMaxVolume:
			SaveUndo();
			pmx::MaxVolume(fSound, from, to);
			Apply();
			break;
		case kMsgMetal:
			SaveUndo();
			pmx::Metal(fSound, from, to);
			Apply();
			break;
		case kMsgVolume:
			AskSettings(kMsgVolume, B_TRANSLATE("Volume"), false, true);
			break;
		case kMsgFade:
			AskSettings(kMsgFade, B_TRANSLATE("Final"), false, true);
			break;
		case kMsgEcho:
			AskSettings(kMsgEcho, B_TRANSLATE("Echo"), true, false);
			break;
		case kMsgChorus:
			AskSettings(kMsgChorus, B_TRANSLATE("Chorus"), true, false);
			break;
		case kMsgCompress:
			AskSettings(kMsgCompress, B_TRANSLATE("Compress"), false, false);
			break;
		case kMsgSettings:
		{
			int32 filter;
			float v1, v2;
			if (message->FindInt32("filter", &filter) != B_OK
				|| message->FindFloat("value1", &v1) != B_OK
				|| message->FindFloat("value2", &v2) != B_OK)
				break;
			fValue1 = v1;
			if (filter == kMsgEcho || filter == kMsgChorus)
				fValue2 = v2;
			SaveUndo();
			switch (filter) {
				case kMsgVolume:
					pmx::ChangeVolume(fSound, from, to, v1);
					break;
				case kMsgFade:
					pmx::Fade(fSound, from, to, v1);
					break;
				case kMsgEcho:
					pmx::Echo(fSound, from, to, v1, v2);
					break;
				case kMsgChorus:
					pmx::Chorus(fSound, from, to, v1);
					break;
				case kMsgCompress:
					pmx::Compress(fSound, from, to, v1);
					break;
			}
			Apply();
			break;
		}
		default:
			BWindow::MessageReceived(message);
	}
}
