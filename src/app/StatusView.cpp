// StatusView.cpp: see StatusView.h.
#include "StatusView.h"

#include <Looper.h>
#include <Polygon.h>
#include <Window.h>

#include "Palette.h"
#include "PowerMix.h"

static const rgb_color kStripGrey = {120, 120, 120, 255};

// #pragma mark - MasterSlider

MasterSlider::MasterSlider(BRect frame, BHandler* target)
	:
	BView(frame, "master", B_FOLLOW_LEFT | B_FOLLOW_TOP, B_WILL_DRAW),
	fTarget(target),
	fValue(50),
	fTracking(false),
	fGrab(0)
{
	SetViewColor(B_TRANSPARENT_COLOR);
}

float
MasterSlider::ThumbY() const
{
	return 15 + fValue / 100.0f * 140.0f;
}

void
MasterSlider::SetValue(float value, bool notify)
{
	if (value < 0)
		value = 0;
	if (value > 100)
		value = 100;
	if (value == fValue)
		return;
	fValue = value;
	Invalidate();
	if (notify && fTarget != NULL && Looper() != NULL) {
		BMessage message(kMsgMasterChanged);
		message.AddFloat("value", fValue);
		Looper()->PostMessage(&message, fTarget);
	}
}

// TSlider::DrawBack: grey ground, small ticks every 10, a bevelled rail,
// bars every 20 (wider at the ends), "100%" and "0%"
void
MasterSlider::Draw(BRect updateRect)
{
	SetHighColor(kStripGrey);
	FillRect(Bounds());

	SetHighColor(20, 20, 20);
	for (int y = 0; y < 0x8d; y += 10)
		StrokeLine(BPoint(15, y + 15), BPoint(24, y + 15));

	SetHighColor(110, 110, 110);
	StrokeLine(BPoint(20, 15), BPoint(20, 155));
	SetHighColor(220, 220, 220);
	StrokeLine(BPoint(19, 15), BPoint(19, 155));
	SetHighColor(30, 30, 30);
	StrokeLine(BPoint(21, 15), BPoint(21, 155));

	for (int y = 1; y < 0x8e; y += 20) {
		float half = (y == 1 || y == 0x8d) ? 14 : 10;
		float left = 20 - half, right = 20 + half;
		float row = y + 15 - 1;
		SetHighColor(130, 130, 130);
		StrokeLine(BPoint(left, row + 1), BPoint(right, row + 1));
		SetHighColor(220, 220, 220);
		StrokeLine(BPoint(left, row), BPoint(right, row));
		SetHighColor(30, 30, 30);
		StrokeLine(BPoint(left, row + 2), BPoint(right, row + 2));
		StrokeLine(BPoint(right, row), BPoint(right, row + 2));
	}
	SetHighColor(130, 130, 130);
	StrokeLine(BPoint(20, 17), BPoint(20, 153));
	SetHighColor(220, 220, 220);
	StrokeLine(BPoint(19, 17), BPoint(19, 153));

	BFont font;
	GetFont(&font);
	font.SetSize(10);
	SetFont(&font);
	SetHighColor(10, 10, 10);
	SetLowColor(kStripGrey);
	DrawString("100%", BPoint(7, 10));
	DrawString("0%", BPoint(15, 170));

	DrawThumb(ThumbY());
}

// A bevelled diamond on the rail, drawn to match 3dmiX's thumb image
void
MasterSlider::DrawThumb(float y)
{
	const float cx = 20;
	SetDrawingMode(B_OP_ALPHA);
	SetHighColor(0, 0, 0, 110);
	BPoint shadow[4] = {BPoint(cx + 3, y - 11), BPoint(cx + 17, y + 3), BPoint(cx + 3, y + 17),
		BPoint(cx - 11, y + 3)};
	FillPolygon(shadow, 4);
	SetDrawingMode(B_OP_COPY);

	BPoint top(cx, y - 14), left(cx - 14, y), right(cx + 14, y), bottom(cx, y + 14);
	BPoint upperLeft[3] = {top, left, BPoint(cx, y)};
	BPoint upperRight[3] = {top, right, BPoint(cx, y)};
	BPoint lowerLeft[3] = {left, bottom, BPoint(cx, y)};
	BPoint lowerRight[3] = {right, bottom, BPoint(cx, y)};
	SetHighColor(248, 248, 248);
	FillPolygon(upperLeft, 3);
	SetHighColor(200, 200, 200);
	FillPolygon(upperRight, 3);
	SetHighColor(176, 176, 176);
	FillPolygon(lowerLeft, 3);
	SetHighColor(56, 56, 56);
	FillPolygon(lowerRight, 3);
	// the notch across the middle
	SetHighColor(30, 30, 30);
	StrokeLine(BPoint(cx - 6, y - 1), BPoint(cx + 6, y - 1));
	SetHighColor(240, 240, 240);
	StrokeLine(BPoint(cx - 10, y + 1), BPoint(cx + 10, y + 1));
}

// TSlider::MouseDown: the thumb follows the pointer while the button is
// down (grabbing it where it was clicked)
void
MasterSlider::MouseDown(BPoint where)
{
	SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS);
	float offset = where.y - ThumbY();
	fGrab = (offset >= -14 && offset <= 14) ? offset : 0;
	fTracking = true;
	SetValue((where.y - fGrab - 15) / 140.0f * 100.0f);
}

void
MasterSlider::MouseMoved(BPoint where, uint32 transit, const BMessage* drag)
{
	if (fTracking)
		SetValue((where.y - fGrab - 15) / 140.0f * 100.0f);
}

void
MasterSlider::MouseUp(BPoint where)
{
	fTracking = false;
}

// #pragma mark - StatusView

StatusView::StatusView(BRect frame, BHandler* target)
	:
	BView(frame, "status", B_FOLLOW_LEFT | B_FOLLOW_TOP_BOTTOM, B_WILL_DRAW),
	fLeft(0),
	fRight(0),
	fLeftPeak(0),
	fRightPeak(0),
	fCount(0),
	fTracking(false),
	fLastY(0),
	fScroll(0)
{
	SetViewColor(kStripGrey);
	fSlider = new MasterSlider(BRect(12, 200, 51, 375), target);
	AddChild(fSlider);
}

void
StatusView::Draw(BRect updateRect)
{
	SetDrawingMode(B_OP_COPY);
	DrawMeter();
	SetDrawingMode(B_OP_OVER);
	SetHighColor(20, 20, 20);
	DrawString("L", BPoint(20, 165));
	DrawString("R", BPoint(41, 165));
	SetDrawingMode(B_OP_COPY);
}

// StatusView::calc_color through pmap(): 16 blue steps, then purple to red
static rgb_color
SegmentColor(int i)
{
	static const uint8 kBlue[16] = {0x64, 0x73, 0x7f, 0x8b, 0x97, 0xa3, 0xaf, 0xc5, 0xd1,
		0xdd, 0xe9, 0xf5, 0xff, 0xff, 0xff, 0xff};
	static const uint8 kHot[9][3] = {{0x6e, 0x1e, 0x5a}, {0x87, 0x1e, 0x32}, {0xa0, 0x1e, 0x1e},
		{0xaa, 0x0a, 0x0a}, {0xbe, 0, 0}, {0xd2, 0, 0}, {0xe6, 0, 0}, {0xff, 0, 0}, {0xff, 0, 0}};
	uint8 r = 0, g = 0, b = 0;
	if (i < 16)
		b = kBlue[i];
	else {
		r = kHot[i - 16][0];
		g = kHot[i - 16][1];
		b = kHot[i - 16][2];
	}
	rgb_color color = {pmx::Gamma(r), pmx::Gamma(g), pmx::Gamma(b), 255};
	return color;
}

// StatusView::drawer: 24 segments per side, 1000 units each, and the peak
void
StatusView::DrawMeter()
{
	const float columns[2][2] = {{12.979f, 28.979f}, {33.957f, 49.957f}};
	const float levels[2] = {fLeft, fRight};
	const float peaks[2] = {fLeftPeak, fRightPeak};
	for (int side = 0; side < 2; side++) {
		for (int i = 0; i < 24; i++) {
			float top = (24 - i) * 5 + 30;
			if (levels[side] / 24000.0f * 24.0f <= float(i))
				SetHighColor(0, 0x50, 0);
			else
				SetHighColor(SegmentColor(i));
			FillRect(BRect(columns[side][0], top, columns[side][1], top + 3));
		}
		float peak = peaks[side] / 24000.0f * 24.0f;
		if (peak >= 23)
			peak = 23;
		int segment = int(peak);
		float top = (24 - segment) * 5 + 30;
		SetHighColor(140, 140, 160);
		FillRect(BRect(columns[side][0], top, columns[side][1], top + 3));
	}
}

void
StatusView::SetLevel(float left, float right)
{
	fLeftPeak *= 0.95f;
	fRightPeak *= 0.95f;
	if (fLeftPeak < left)
		fLeftPeak = left;
	if (fRightPeak < right)
		fRightPeak = right;
	fLeft = (fLeft + left) * 0.5f;
	fRight = (fRight + right) * 0.5f;
	uint32 count = fCount++;
	if (fLeftPeak >= 1000 || fRightPeak >= 1000 || (count & 1) != 0)
		DrawMeter();
}

// StatusView::MouseDown: dragging scrolls the strip's contents up to 200
// pixels
void
StatusView::MouseDown(BPoint where)
{
	SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS);
	fTracking = true;
	fLastY = where.y;
}

void
StatusView::MouseMoved(BPoint where, uint32 transit, const BMessage* drag)
{
	if (!fTracking)
		return;
	int32 delta = int32(-(where.y - fLastY));
	if (delta == 0)
		return;
	int32 scroll = fScroll + delta * 2;
	if (scroll < -200)
		scroll = -200;
	if (scroll > 0)
		scroll = 0;
	int32 moved = scroll - fScroll;
	ScrollBy(0, moved);
	fScroll = scroll;
	fLastY = where.y + moved;
}

void
StatusView::MouseUp(BPoint where)
{
	fTracking = false;
}
