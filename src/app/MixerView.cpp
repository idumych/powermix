// MixerView.cpp: see MixerView.h. The behaviour follows TSoundView:
// MouseDown/handle_hit, KeyDown, mute_selection, solo_selection, Animate.
#include "MixerView.h"

#include <Bitmap.h>
#include <GradientLinear.h>
#include <GradientRadial.h>
#include <Catalog.h>
#include <Region.h>
#include <Window.h>

#include <algorithm>
#include <cmath>
#include <cstring>

#include "AudioEngine.h"
#include "Look.h"
#include "ModernViews.h"
#include "Palette.h"
#include "PowerMix.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "MixerView"

MixerView::MixerView(BRect frame, AudioEngine* engine)
	:
	BView(frame, "mixer", B_FOLLOW_NONE,
		B_WILL_DRAW | B_FRAME_EVENTS | B_FULL_UPDATE_ON_RESIZE),
	fEngine(engine),
	fDisplay(NULL),
	fDisplayView(NULL),
	fButtons(NULL),
	fMargin(make_color(0, 0, 0)),
	fModern(false),
	fShowLabels(true),
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
	delete fDisplay;
	delete[] fButtons;
}

void
MixerView::SetModern(bool modern)
{
	if (modern == fModern)
		return;
	fModern = modern;
	if (fModern && fButtons == NULL) {
		// zoom in, zoom out, reset the angle: top right
		const ModernButton::Icon icons[3] = {ModernButton::kIconPlus, ModernButton::kIconMinus,
			ModernButton::kIconReset};
		const char* names[3] = {"zoom in", "zoom out", "reset view"};
		fButtons = new ModernButton*[3];
		for (int i = 0; i < 3; i++) {
			BMessage* message = new BMessage(kMsgZoomStage);
			message->AddInt32("dir", i == 0 ? 1 : i == 1 ? -1 : 0);
			fButtons[i] = new ModernButton(BRect(0, 0, 33, 33), names[i], "", message, icons[i],
				ModernButton::kOverlay, B_FOLLOW_RIGHT | B_FOLLOW_TOP);
			AddChild(fButtons[i]);
		}
	}
	if (fButtons != NULL) {
		for (int i = 0; i < 3; i++) {
			fButtons[i]->MoveTo(Bounds().right - 12 - 33, 12 + i * 40);
			if (fModern && fButtons[i]->IsHidden())
				fButtons[i]->Show();
			else if (!fModern && !fButtons[i]->IsHidden())
				fButtons[i]->Hide();
		}
	}
	if (!fModern) {
		delete fDisplay;
		fDisplay = NULL;
		fDisplayView = NULL;
	}
	Render();
	Invalidate();
}

void
MixerView::FrameResized(float width, float height)
{
	if (!fModern)
		return;
	delete fDisplay;
	fDisplay = NULL;
	fDisplayView = NULL;
	Render();
	Invalidate();
}

void
MixerView::Zoom(int32 direction)
{
	if (direction > 0)
		fScene.ZoomIn();
	else if (direction < 0)
		fScene.ZoomOut();
	else {
		fScene.SetSpin(0, 0);
		fScene.SetCamera(0.32f, 0.1f);
	}
	Render();
	Invalidate();
}

// The frame's place: centred across the window in the classic look,
// scaled to fit the view in the modern one
BRect
MixerView::FrameRect() const
{
	if (!fModern) {
		float x = FrameOffset();
		return BRect(x, 0, x + pmx::Scene::kWidth - 1, pmx::Scene::kHeight - 1);
	}
	BRect b = Bounds();
	float scale = std::min((b.Width() + 1) / pmx::Scene::kWidth,
		(b.Height() + 1) / pmx::Scene::kHeight);
	float w = floorf(pmx::Scene::kWidth * scale), h = floorf(pmx::Scene::kHeight * scale);
	float left = floorf((b.Width() + 1 - w) / 2), top = floorf((b.Height() + 1 - h) / 2);
	return BRect(left, top, left + w - 1, top + h - 1);
}

BPoint
MixerView::ToFrame(BPoint where) const
{
	if (fModern) {
		float scale;
		BPoint offset;
		ModernTransform(scale, offset);
		return BPoint((where.x - offset.x) / scale, (where.y - offset.y) / scale);
	}
	BRect frame = FrameRect();
	float scale = (frame.Width() + 1) / pmx::Scene::kWidth;
	return BPoint((where.x - frame.left) / scale, (where.y - frame.top) / scale);
}

bool
MixerView::FloorUnderPointer(float& x, float& y)
{
	BPoint where;
	uint32 buttons;
	GetMouse(&where, &buttons, false);
	if (!Bounds().Contains(where))
		return false;
	BPoint p = ToFrame(where);
	return fScene.FloorPoint(p.x, p.y, x, y);
}

int
MixerView::FrameOffset() const
{
	return Window() != NULL ? pmx::Scene::FrameOffset(Window()->Bounds().Width()) : 0;
}

void
MixerView::Draw(BRect updateRect)
{
	if (fModern && fDisplay != NULL)
		DrawBitmap(fDisplay, BPoint(0, 0));
	else
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
	if (fModern) {
		ComposeModern();
		if (fButtons != NULL) {
			for (int i = 0; i < 3; i++)
				fButtons[i]->SetBackground(fMargin);
		}
		return;
	}
	fScene.Render(LabelWidth);
	// the modern look lifts black to the stage's navy
	static uint32 sColors[2][256];
	static bool sReady = false;
	if (!sReady) {
		const rgb_color stage = look::kStage;
		for (int i = 0; i < 256; i++) {
			const pmx::Rgb& c = pmx::kSystemPalette[i];
			sColors[0][i] = 0xff000000 | uint32(c.r) << 16 | uint32(c.g) << 8 | c.b;
			uint32 r = stage.red + c.r * (255 - stage.red) / 255;
			uint32 g = stage.green + c.g * (255 - stage.green) / 255;
			uint32 b = stage.blue + c.b * (255 - stage.blue) / 255;
			sColors[1][i] = 0xff000000 | r << 16 | g << 8 | b;
		}
		sReady = true;
	}
	const uint32* colors = sColors[fModern ? 1 : 0];
	const uint8* frame = fScene.Frame();
	const uint32 margin = colors[frame[0]];
	fMargin = make_color((margin >> 16) & 0xff, (margin >> 8) & 0xff, margin & 0xff);
	uint8* bits = static_cast<uint8*>(fBitmap->Bits());
	const int32 bpr = fBitmap->BytesPerRow();
	for (int y = 0; y < pmx::Scene::kHeight; y++) {
		uint32* row = reinterpret_cast<uint32*>(bits + y * bpr);
		const uint8* in = frame + y * pmx::Scene::kWidth;
		for (int x = 0; x < pmx::Scene::kWidth; x++)
			row[x] = colors[in[x]];
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

// The modern look's view of the scene: the frame's camera, scaled so the
// floor fills the view (and the zoom keys still zoom). A circle on the
// floor turns into itself as the view turns, so only tilting rescales.
void
MixerView::ModernTransform(float& scale, BPoint& offset) const
{
	BRect b = Bounds();
	float left = 1e9f, top = 1e9f, right = -1e9f, bottom = -1e9f;
	for (int k = 0; k < 24; k++) {
		const float angle = k * 3.1415926f / 12;
		for (float y : {0.0f, -120.0f}) {
			float fx, fy, depth;
			fScene.ProjectPoint(cosf(angle) * 300, y, sinf(angle) * 300, fx, fy, depth);
			left = std::min(left, fx);
			right = std::max(right, fx);
			top = std::min(top, fy);
			bottom = std::max(bottom, fy);
		}
	}
	const float width = std::max(1.0f, right - left), height = std::max(1.0f, bottom - top);
	const float fit = std::min((b.Width() - 40) / width, (b.Height() - 60) / height);
	// the fit undoes the focal length; put it back so + and - zoom
	scale = std::max(0.05f, fit * fScene.Focal() / 300);
	offset.x = b.Width() / 2 - (left + right) / 2 * scale;
	offset.y = 20 + (b.Height() - 40) / 2 - (top + bottom) / 2 * scale;
}

// The modern picture, drawn as vectors at the view's size and composed
// offscreen so the animation doesn't flicker: a dark stage with a glow, the
// floor's grid fading into the distance, a shadow under each column,
// translucent columns with their level bars, the names as pills
void
MixerView::ComposeModern()
{
	BRect bounds = Bounds();
	bounds.OffsetTo(0, 0);
	if (!bounds.IsValid() || bounds.Width() < 8 || bounds.Height() < 8)
		return;
	if (fDisplay == NULL) {
		fDisplay = new BBitmap(bounds, B_RGB32, true);
		fDisplayView = new BView(bounds, "display", B_FOLLOW_NONE, 0);
		fDisplay->AddChild(fDisplayView);
	}
	if (!fDisplay->Lock())
		return;
	BView* v = fDisplayView;
	float scale;
	BPoint offset;
	ModernTransform(scale, offset);
	auto at = [scale, offset](float x, float y) {
		return BPoint(offset.x + x * scale, offset.y + y * scale);
	};

	// the stage: dark at the top, deep blue towards the floor, a glow
	const rgb_color topColor = {8, 10, 16, 255};
	fMargin = topColor;
	BGradientLinear ground(BPoint(0, 0), BPoint(0, bounds.bottom));
	ground.AddColor(topColor, 0);
	ground.AddColor(make_color(11, 16, 30, 255), 140);
	ground.AddColor(make_color(15, 28, 60, 255), 255);
	v->SetDrawingMode(B_OP_COPY);
	v->FillRect(bounds, ground);
	v->SetDrawingMode(B_OP_ALPHA);
	v->SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
	float cx, cy, depth;
	fScene.ProjectPoint(0, 0, 0, cx, cy, depth);
	BPoint center = at(cx, cy);
	const float glow = std::max(bounds.Width(), bounds.Height()) * 0.55f;
	BGradientRadial halo(center, glow);
	halo.AddColor(make_color(40, 80, 170, 70), 0);
	halo.AddColor(make_color(40, 80, 170, 0), 255);
	v->FillEllipse(center, glow, glow, halo);

	std::vector<pmx::Scene::FrameLine> lines;
	std::vector<pmx::Scene::FrameFace> faces;
	std::vector<pmx::Scene::FrameLabel> labels;
	fScene.ProjectScene(lines, faces, labels);

	// the floor and its grid, brighter nearer the listener
	BPoint floorShape[4];
	const float corners[4][2] = {{-256, -256}, {256, -256}, {256, 256}, {-256, 256}};
	for (int k = 0; k < 4; k++) {
		float fx, fy;
		fScene.ProjectPoint(corners[k][0], 0, corners[k][1], fx, fy, depth);
		floorShape[k] = at(fx, fy);
	}
	v->SetHighColor(25, 50, 120, 60);
	v->FillPolygon(floorShape, 4);
	for (const pmx::Scene::FrameLine& line : lines) {
		const float near = std::max(0.0f, std::min(1.0f, (line.depth + 280) / 560));
		if (line.kind == 0)
			v->SetHighColor(91, 155, 255, uint8(25 + near * 130));
		else
			v->SetHighColor(154, 163, 178, 150);
		v->StrokeLine(at(line.x1, line.y1), at(line.x2, line.y2));
	}

	// shadows: a circle on the floor under each column
	for (int i = 0; i < fScene.CountTracks(); i++) {
		float x, y;
		fScene.GetPosition(i, x, y);
		BPoint circle[16];
		for (int k = 0; k < 16; k++) {
			float fx, fy;
			const float angle = k * 3.1415926f / 8;
			fScene.ProjectPoint(x + cosf(angle) * 22, 0, y + sinf(angle) * 22, fx, fy, depth);
			circle[k] = at(fx, fy);
		}
		rgb_color color = look::TrackColor(i);
		color.alpha = fScene.IsMuted(i) ? 18 : 50;
		v->SetHighColor(color);
		v->FillPolygon(circle, 16);
	}

	// the columns, far to near
	const rgb_color grey = {120, 128, 142, 255};
	for (const pmx::Scene::FrameFace& face : faces) {
		if (face.track < 0 || face.track >= fScene.CountTracks())
			continue;
		const bool selected = fScene.IsSelected(face.track);
		const bool muted = fScene.IsMuted(face.track);
		rgb_color color = look::TrackColor(face.track);
		if (muted)
			color = look::Mix(color, grey, 0.75f);
		BPoint points[4];
		for (int k = 0; k < 4; k++)
			points[k] = at(face.x[k], face.y[k]);
		uint8 alpha;
		switch (face.part) {
			case 0: alpha = 150; break;
			case 2: alpha = 30; break;
			case 3: alpha = 235; break;
			default: alpha = 70; break;
		}
		if (selected && face.part != 3)
			alpha = uint8(std::min(255, alpha + 40));
		rgb_color fill = face.part == 3 ? look::Mix(color, make_color(255, 255, 255), 0.25f)
			: color;
		fill.alpha = alpha;
		v->SetHighColor(fill);
		v->FillPolygon(points, 4);
		if (face.part == 3)
			continue;
		rgb_color edge = selected ? make_color(255, 255, 255, 220) : color;
		if (!selected)
			edge.alpha = 170;
		v->SetHighColor(edge);
		v->SetPenSize(selected ? 1.5f : 1.0f);
		v->StrokePolygon(points, 4);
		v->SetPenSize(1);
	}

	// the names, on a thin stem above each column
	BFont font(be_bold_font);
	font.SetSize(11);
	v->SetFont(&font);
	font_height fh;
	font.GetHeight(&fh);
	std::vector<std::string> trackNames;
	if (fEngine->Lock()) {
		for (const pmx::Track& track : fEngine->Project().tracks)
			trackNames.push_back(track.name);
		fEngine->Unlock();
	}
	if (fShowLabels) {
		for (const pmx::Scene::FrameLabel& label : labels) {
			if (label.track < 0 || size_t(label.track) >= trackNames.size())
				continue;
			const bool selected = fScene.IsSelected(label.track);
			const char* text = trackNames[size_t(label.track)].c_str();
			BPoint anchor = at(label.anchorX, label.anchorY);
			BPoint top = at(label.x, label.y);
			top.y = std::min(top.y, anchor.y - 14);
			v->SetHighColor(154, 163, 178, 90);
			v->StrokeLine(anchor, BPoint(anchor.x, top.y));
			const float w = font.StringWidth(text) + 16;
			BRect pill(floorf(top.x - w / 2), floorf(top.y - fh.ascent - fh.descent - 8),
				floorf(top.x + w / 2), floorf(top.y));
			if (selected) {
				look::FillRound(v, pill, 9, look::kAccent);
				v->SetHighColor(look::kGround);
			} else {
				look::FillRound(v, pill, 9, make_color(12, 15, 22, 215));
				look::StrokeRound(v, pill, 9, look::kBorder);
				v->SetHighColor(215, 220, 229);
			}
			v->DrawString(text, BPoint(pill.left + 8, pill.bottom - 4 - fh.descent));
		}
	}

	const char* hint = B_TRANSLATE("Drag a track to place it · drag the floor to turn the view");
	BFont plain(be_plain_font);
	plain.SetSize(12);
	v->SetFont(&plain);
	const float hw = plain.StringWidth(hint) + 20;
	if (hw + 80 < bounds.Width() && bounds.Height() > 160) {
		// bottom left, out of the labels' way
		BRect box(12, bounds.bottom - 39, 12 + hw, bounds.bottom - 12);
		look::FillRound(v, box, 8, make_color(12, 15, 22, 200));
		look::StrokeRound(v, box, 8, make_color(38, 44, 57, 255));
		v->SetHighColor(look::kMuted);
		v->DrawString(hint, BPoint(22, box.bottom - 9));
	}
	v->SetDrawingMode(B_OP_COPY);
	v->Sync();
	fDisplay->Unlock();
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

	BPoint inFrame = ToFrame(where);
	int hit = fScene.HitTest(inFrame.x, inFrame.y);
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

// TSoundView::MessageDropped: a dropped file becomes a track where it
// lands on the floor
void
MixerView::MessageReceived(BMessage* message)
{
	if (message->WasDropped() && message->HasRef("refs")) {
		BPoint where = ConvertFromScreen(message->DropPoint());
		float x = 0, y = 100;
		BPoint inFrame = ToFrame(where);
		fScene.FloorPoint(inFrame.x, inFrame.y, x, y);
		BMessage drop(*message);
		drop.what = kMsgDropRefs;
		drop.AddInt32("lane", -1);
		drop.AddFloat("x", x);
		drop.AddFloat("y", y);
		Window()->PostMessage(&drop);
		return;
	}
	BView::MessageReceived(message);
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
