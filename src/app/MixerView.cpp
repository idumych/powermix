// MixerView.cpp: see MixerView.h. The behaviour follows TSoundView:
// MouseDown/handle_hit, KeyDown, mute_selection, solo_selection, Animate.
#include "MixerView.h"

#include <Bitmap.h>
#include <Window.h>

#include <cstring>

#include "AudioEngine.h"
#include "Palette.h"
#include "PowerMix.h"

MixerView::MixerView(BRect frame, AudioEngine* engine)
	:
	BView(frame, "mixer", B_FOLLOW_NONE, B_WILL_DRAW),
	fEngine(engine),
	fSelectionTarget(NULL),
	fActive(false),
	fTracking(kNone),
	fLastPitch(0),
	fLastYaw(0),
	fPrevPitch(0),
	fPrevYaw(0),
	fMoved(false),
	fDepthOnly(false)
{
	SetViewColor(B_TRANSPARENT_COLOR);
	BRect bounds(0, 0, pmx::Scene::kWidth - 1, pmx::Scene::kHeight - 1);
	fBitmap = new BBitmap(bounds, B_RGB32, true);
	fBitmapView = new BView(bounds, "labels", B_FOLLOW_NONE, 0);
	fBitmap->AddChild(fBitmapView);
	fBitmap->Lock();
	fBitmapView->SetFont(be_plain_font);
	fBitmapView->SetDrawingMode(B_OP_OVER);
	fBitmap->Unlock();
	Render();
}

MixerView::~MixerView()
{
	delete fBitmap;
}

bool
MixerView::FloorUnderPointer(float& x, float& y)
{
	BPoint where;
	uint32 buttons;
	GetMouse(&where, &buttons, false);
	if (!Bounds().Contains(where))
		return false;
	return fScene.FloorPoint(where.x - FrameOffset(), where.y, x, y);
}

int
MixerView::FrameOffset() const
{
	return Window() != NULL ? pmx::Scene::FrameOffset(Window()->Bounds().Width()) : 0;
}

void
MixerView::Draw(BRect updateRect)
{
	DrawBitmap(fBitmap, BPoint(FrameOffset(), 0));
}

void
MixerView::SyncTracks()
{
	pmx::Project& project = fEngine->Project();
	fScene.RemoveAllTracks();
	for (const pmx::Track& track : project.tracks) {
		if (track.addedByHand)
			fScene.AddNewTrack(track.name, track.x, track.y);
		else
			fScene.AddTrack(track.name, track.x, track.y);
	}
	for (int i = 0; i < fScene.CountTracks(); i++)
		fScene.SetMuted(i, fEngine->Mixer().IsMuted(size_t(i)));
}

// The scene owns the tracks' places on the floor; the mix reads them from
// the project
void
MixerView::WritePositions()
{
	pmx::Project& project = fEngine->Project();
	for (int i = 0; i < fScene.CountTracks() && i < int(project.tracks.size()); i++)
		fScene.GetPosition(i, project.tracks[size_t(i)].x, project.tracks[size_t(i)].y);
}

void
MixerView::Animate(bool fast)
{
	if (fEngine->Lock()) {
		pmx::Mixer& mixer = fEngine->Mixer();
		bool anySolo = false;
		for (int i = 0; i < fScene.CountTracks(); i++)
			anySolo |= mixer.IsSoloed(size_t(i));
		for (int i = 0; i < fScene.CountTracks(); i++) {
			fScene.SetMuted(i, mixer.IsMuted(size_t(i)));
			fScene.SetSoloSilenced(i, anySolo && !mixer.IsSoloed(size_t(i)));
			fScene.SetLevel(i, mixer.TrackLevel(size_t(i)));
		}
		WritePositions();
		fEngine->Unlock();
	}
	if (!fast || fTracking != kNone)
		return;
	fScene.Step();
	Render();
	Invalidate();
}

static float
LabelWidth(const std::string& text)
{
	return be_plain_font->StringWidth(text.c_str());
}

void
MixerView::Render()
{
	fScene.Render(LabelWidth);
	static uint32 sColors[256];
	static bool sReady = false;
	if (!sReady) {
		for (int i = 0; i < 256; i++) {
			const pmx::Rgb& c = pmx::kSystemPalette[i];
			sColors[i] = 0xff000000 | uint32(c.r) << 16 | uint32(c.g) << 8 | c.b;
		}
		sReady = true;
	}
	const uint8* frame = fScene.Frame();
	uint8* bits = static_cast<uint8*>(fBitmap->Bits());
	const int32 bpr = fBitmap->BytesPerRow();
	for (int y = 0; y < pmx::Scene::kHeight; y++) {
		uint32* row = reinterpret_cast<uint32*>(bits + y * bpr);
		const uint8* in = frame + y * pmx::Scene::kWidth;
		for (int x = 0; x < pmx::Scene::kWidth; x++)
			row[x] = sColors[in[x]];
	}
	// draw_labels: (0xaa, 0xa0, 0xa0), a selected track's (0xff, 0xa0, 0xa0)
	fBitmap->Lock();
	for (const pmx::Scene::Label& label : fScene.Labels()) {
		fBitmapView->SetHighColor(label.selected ? 0xff : 0xaa, 0xa0, 0xa0);
		fBitmapView->DrawString(label.text.c_str(), BPoint(label.x, label.y));
	}
	fBitmapView->Sync();
	fBitmap->Unlock();
}

void
MixerView::SelectionChanged()
{
	if (fSelectionTarget == NULL || Looper() == NULL)
		return;
	BMessage message(kMsgMixerSelection);
	for (int i = 0; i < fScene.CountTracks(); i++) {
		if (fScene.IsSelected(i))
			message.AddInt32("track", i);
	}
	Looper()->PostMessage(&message, fSelectionTarget);
}

// MouseDown: a track selects it (Shift adds/removes) and drags the
// selection; Shift-drag moves in depth only. Empty floor turns the camera,
// which keeps spinning if the mouse was still moving when let go.
void
MixerView::MouseDown(BPoint where)
{
	MakeFocus(true);
	uint32 modifiers = 0;
	if (Window()->CurrentMessage() != NULL)
		Window()->CurrentMessage()->FindInt32("modifiers", (int32*)&modifiers);
	SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS);
	fLast = where;
	fMoved = false;

	int hit = fScene.HitTest(where.x - FrameOffset(), where.y);
	if (hit < 0) {
		fScene.DeselectAll();
		SelectionChanged();
		fTracking = kRotating;
		fLastPitch = fPrevPitch = fScene.Pitch();
		fLastYaw = fPrevYaw = fScene.Yaw();
		return;
	}
	if ((modifiers & B_SHIFT_KEY) != 0)
		fScene.Select(hit, !fScene.IsSelected(hit));
	else if (!fScene.IsSelected(hit)) {
		fScene.DeselectAll();
		fScene.Select(hit, true);
	}
	SelectionChanged();
	fDepthOnly = (modifiers & B_SHIFT_KEY) != 0;
	fTracking = kDragging;
	Render();
	Invalidate();
}

void
MixerView::MouseMoved(BPoint where, uint32 transit, const BMessage* drag)
{
	if (fTracking == kNone)
		return;
	float dx = where.x - fLast.x;
	float dy = where.y - fLast.y;
	fLast = where;
	if (dx == 0 && dy == 0)
		return;
	if (fTracking == kRotating) {
		fPrevPitch = fScene.Pitch();
		fPrevYaw = fScene.Yaw();
		fScene.Rotate(dx, dy);
		fMoved = true;
	} else {
		fScene.DragSelection(dx, dy, fDepthOnly);
		if (fEngine->Lock()) {
			WritePositions();
			fEngine->Unlock();
		}
	}
	Render();
	Invalidate();
}

void
MixerView::MouseUp(BPoint where)
{
	if (fTracking == kRotating && fMoved)
		fScene.SetSpin(fPrevPitch - fScene.Pitch(), fPrevYaw - fScene.Yaw());
	fTracking = kNone;
}

void
MixerView::MuteSelection()
{
	if (!fEngine->Lock())
		return;
	pmx::Mixer& mixer = fEngine->Mixer();
	bool anyUnmuted = false;
	for (int i = 0; i < fScene.CountTracks(); i++)
		anyUnmuted |= fScene.IsSelected(i) && !mixer.IsMuted(size_t(i));
	for (int i = 0; i < fScene.CountTracks(); i++) {
		if (fScene.IsSelected(i))
			mixer.SetMuted(size_t(i), anyUnmuted);
	}
	fEngine->Unlock();
	if (Looper() != NULL)
		Looper()->PostMessage(kMsgMuteSoloChanged);
}

// solo_selection: the last selected track decides whether the selection
// is soloed or unsoloed
void
MixerView::SoloSelection()
{
	if (!fEngine->Lock())
		return;
	pmx::Mixer& mixer = fEngine->Mixer();
	bool solo = false;
	for (int i = 0; i < fScene.CountTracks(); i++) {
		if (fScene.IsSelected(i))
			solo = !mixer.IsSoloed(size_t(i));
	}
	for (int i = 0; i < fScene.CountTracks(); i++) {
		if (fScene.IsSelected(i))
			mixer.SetSoloed(size_t(i), solo);
	}
	fEngine->Unlock();
	if (Looper() != NULL)
		Looper()->PostMessage(kMsgMuteSoloChanged);
}

// TSoundView::KeyDown
void
MixerView::KeyDown(const char* bytes, int32 count)
{
	switch (bytes[0]) {
		case B_LEFT_ARROW:
			fScene.MoveSelection(-15, 0);
			break;
		case B_RIGHT_ARROW:
			fScene.MoveSelection(15, 0);
			break;
		case B_UP_ARROW:
			fScene.MoveSelection(0, -15);
			break;
		case B_DOWN_ARROW:
			fScene.MoveSelection(0, 15);
			break;
		case '+':
		case '=':
			fScene.ZoomIn();
			break;
		case '-':
		case '_':
			fScene.ZoomOut();
			break;
		case ',':
			fScene.MoveCameraBack();
			break;
		case '.':
			fScene.MoveCameraNearer();
			break;
		case 'a':
		case 'A':
			fScene.SelectAll();
			SelectionChanged();
			break;
		case 'd':
		case 'D':
			fScene.ToggleDemo();
			break;
		case 'm':
		case 'M':
			MuteSelection();
			break;
		case 'p':
		case 'P':
			fScene.ToggleParticles();
			break;
		case 's':
		case 'S':
			SoloSelection();
			break;
		case 'w':
		case 'W':
			Window()->PostMessage(kMsgShowHideMixer);
			break;
		default:
			BView::KeyDown(bytes, count);
			return;
	}
	if (fEngine->Lock()) {
		WritePositions();
		fEngine->Unlock();
	}
	Render();
	Invalidate();
}
