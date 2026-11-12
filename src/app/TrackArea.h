// TrackArea.h: the lower half of the window, as 3dmiX's TWaveWindow lays
// it out: the Ticks box (TCtrlView), the time ruler (TimeView), the track
// lanes (TrackView, TrackViewer, SimpleObject::Render), the zoom control
// and the scroll bars.
#ifndef POWERMIX_TRACK_AREA_H
#define POWERMIX_TRACK_AREA_H

#include <View.h>

#include <set>

class AudioEngine;
namespace pmx { class Editor; }
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
	void ProjectChanged(bool keepSelection = false);
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
	void SetEditor(pmx::Editor* editor) { fEditor = editor; }
	// The Edit menu's targets: selected clips, the paste position
	void GetEditTargets(std::set<std::pair<size_t, size_t> >& clips, size_t& pasteTrack,
		size_t& pasteAfter) const;
	void ClearClipSelection();
	// Display ▸ Show/hide beat marks (TimeView::Toggle)
	void SetBeatMarks(bool show);
	void SetTicks(float ticks);
	bool BeatMarks() const { return fBeatMarks; }
	std::set<int> SelectedTracks() const;

private:
	friend class TimeRuler;
	friend class TrackLanes;

	void DrawTriangle();

	AudioEngine* fEngine;
	pmx::Editor* fEditor;
	BTextControl* fTicks;
	BMenuField* fBeats;
	TimeRuler* fRuler;
	TrackLanes* fLanes;
	ZoomBar* fZoom;
	BScrollBar* fHorizontal;
	BScrollBar* fVertical;
	float fRotation;
	bool fBeatMarks = true;
	float fSecondsPerPixel;
	float fViewStart;		// seconds at the lanes' left edge
};

#endif
