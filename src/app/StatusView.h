// StatusView.h: the strip left of the tracks (Display ▸ Show/Hide
// Controls): the stereo meter and the master volume slider. Follows
// 3dmiX's StatusView and TSlider.
#ifndef POWERMIX_STATUS_VIEW_H
#define POWERMIX_STATUS_VIEW_H

#include <View.h>

class MasterSlider : public BView {
public:
	MasterSlider(BRect frame, BHandler* target);

	virtual void Draw(BRect updateRect);
	virtual void MouseDown(BPoint where);
	virtual void MouseMoved(BPoint where, uint32 transit, const BMessage* drag);
	virtual void MouseUp(BPoint where);

	// 0 (top, "100%") to 100 (bottom, "0%"), as TSlider's value
	float Value() const { return fValue; }
	// notify: tell the target (not when it's following the other look's
	// slider, which would send the value back)
	void SetValue(float value, bool notify = true);

private:
	float ThumbY() const;
	void DrawThumb(float y);

	BHandler* fTarget;
	float fValue;
	bool fTracking;
	float fGrab;
};

class StatusView : public BView {
public:
	StatusView(BRect frame, BHandler* target);

	virtual void Draw(BRect updateRect);
	virtual void MouseDown(BPoint where);
	virtual void MouseMoved(BPoint where, uint32 transit, const BMessage* drag);
	virtual void MouseUp(BPoint where);

	// StatusView::SetLevel, from the mixer's sum of |sample| >> 8 per block
	void SetLevel(float left, float right);
	MasterSlider* Slider() const { return fSlider; }

private:
	void DrawMeter();

	MasterSlider* fSlider;
	float fLeft, fRight;		// averaged levels
	float fLeftPeak, fRightPeak;
	uint32 fCount;
	bool fTracking;
	float fLastY;
	int32 fScroll;
};

#endif
