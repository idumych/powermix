// MixerView.h: the 3D mixer (TSoundView's view half): shows the core
// Scene, moves tracks and the camera with the mouse and keys.
#ifndef POWERMIX_MIXER_VIEW_H
#define POWERMIX_MIXER_VIEW_H

#include <View.h>

#include "Scene.h"

class AudioEngine;
class BBitmap;
class ModernButton;

class MixerView : public BView {
public:
	MixerView(BRect frame, AudioEngine* engine);
	virtual ~MixerView();

	virtual void Draw(BRect updateRect);
	virtual void MouseDown(BPoint where);
	virtual void MouseMoved(BPoint where, uint32 transit, const BMessage* drag);
	virtual void MouseUp(BPoint where);
	virtual void KeyDown(const char* bytes, int32 count);
	virtual void MessageReceived(BMessage* message);
	virtual void FrameResized(float width, float height);

	// Rebuilds the scene from the project (after loading, adding or
	// deleting tracks). Call with the engine locked.
	void SyncTracks();
	// invert_map: the floor point under the pointer, if it's over the view
	bool FloorUnderPointer(float& x, float& y);
	// One frame of TSoundView::Animate; fast = the mixer is showing
	void Animate(bool fast);

	pmx::Scene& Scene() { return fScene; }
	void SetActive(bool active) { fActive = active; }
	// The selection changed here; the window tells the track view
	void SetSelectionTarget(BHandler* target) { fSelectionTarget = target; }

	// Tracks ▸ (Un)Mute/(Un)Solo Selection, also M and S here
	void MuteSelection();
	void SoloSelection();

	// The modern look: the frame is scaled to fit the view, its colours
	// darkened, the labels drawn as pills, with zoom buttons on top
	void SetModern(bool modern);
	// The buttons' kMsgZoomStage
	void Zoom(int32 direction);
	void Redraw() { Render(); Invalidate(); }

private:
	int FrameOffset() const;
	BRect FrameRect() const;	// where the frame is drawn in the view
	BPoint ToFrame(BPoint where) const;
	void ModernTransform(float& scale, BPoint& offset) const;
	void Render();
	void ComposeModern();
	void WritePositions();
	void SelectionChanged();

	AudioEngine* fEngine;
	pmx::Scene fScene;
	BBitmap* fBitmap;
	BView* fBitmapView;
	BBitmap* fDisplay;		// modern: the view's picture
	BView* fDisplayView;
	ModernButton** fButtons;	// modern: zoom in, zoom out, reset
	rgb_color fMargin;
	bool fModern;
	bool fShowLabels;
	BHandler* fSelectionTarget;
	bool fActive;

	enum { kNone, kRotating, kDragging } fTracking;
	BPoint fLast;
	float fLastPitch, fLastYaw, fPrevPitch, fPrevYaw;
	bool fMoved;
	bool fDepthOnly;
};

#endif
