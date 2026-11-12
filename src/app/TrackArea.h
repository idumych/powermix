// TrackArea.h: the lower half of the window, as 3dmiX's TWaveWindow lays
// it out: the Ticks box (TCtrlView), the time ruler (TimeView), the track
// lanes (TrackView, TrackViewer, SimpleObject::Render), the zoom control
// and the scroll bars.
#ifndef POWERMIX_TRACK_AREA_H
#define POWERMIX_TRACK_AREA_H

#include <View.h>

#include <set>

class AudioEngine;
class BMenuField;
class BScrollBar;
class BTextControl;
class TimeRuler;
class TrackLanes;
class ZoomBar;

class TrackArea : public BView {
public:
	TrackArea(BRect frame, AudioEngine* engine);

	virtual void AttachedToWindow();
	virtual void Draw(BRect updateRect);
	virtual void MouseDown(BPoint where);
	virtual void MessageReceived(BMessage* message);

	// The project changed (loaded, tracks added or removed)
	void ProjectChanged();
	void SetTime(double time);
	// From the 3D view: "track" int32s of the selected tracks
	void SetSelection(const BMessage& tracks);
	void SetControlsRotation(float angle);	// TCtrlView::SetRot
	void RedrawLanes();
	void SetFastDisplay(bool fast);	// Display ▸ Fast display
	bool FastDisplay() const;
	void SetZoom(float secondsPerPixel);
	float Zoom() const { return fSecondsPerPixel; }
	void ScrollTime(float seconds);

	AudioEngine* Engine() const { return fEngine; }

private:
	friend class TimeRuler;
	friend class TrackLanes;

	void DrawTriangle();

	AudioEngine* fEngine;
	BTextControl* fTicks;
	BMenuField* fBeats;
	TimeRuler* fRuler;
	TrackLanes* fLanes;
	ZoomBar* fZoom;
	BScrollBar* fHorizontal;
	BScrollBar* fVertical;
	float fRotation;
	float fSecondsPerPixel;
	float fViewStart;		// seconds at the lanes' left edge
};

#endif
