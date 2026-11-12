// MixerView.h: the 3D mixer (TSoundView's view half): shows the core
// Scene, moves tracks and the camera with the mouse and keys.
#ifndef POWERMIX_MIXER_VIEW_H
#define POWERMIX_MIXER_VIEW_H

#include <View.h>

#include "Scene.h"

class AudioEngine;
class BBitmap;

class MixerView : public BView {
public:
	MixerView(BRect frame, AudioEngine* engine);
	virtual ~MixerView();

	virtual void Draw(BRect updateRect);
	virtual void MouseDown(BPoint where);
	virtual void MouseMoved(BPoint where, uint32 transit, const BMessage* drag);
	virtual void MouseUp(BPoint where);
	virtual void KeyDown(const char* bytes, int32 count);

	// Rebuilds the scene from the project (after loading, adding or
	// deleting tracks). Call with the engine locked.
	void SyncTracks();
	// One frame of TSoundView::Animate; fast = the mixer is showing
	void Animate(bool fast);

	pmx::Scene& Scene() { return fScene; }
	void SetActive(bool active) { fActive = active; }
	// The selection changed here; the window tells the track view
	void SetSelectionTarget(BHandler* target) { fSelectionTarget = target; }

	// Tracks ▸ (Un)Mute/(Un)Solo Selection, also M and S here
	void MuteSelection();
	void SoloSelection();

private:
	int FrameOffset() const;
	void Render();
	void WritePositions();
	void SelectionChanged();

	AudioEngine* fEngine;
	pmx::Scene fScene;
	BBitmap* fBitmap;
	BView* fBitmapView;
	BHandler* fSelectionTarget;
	bool fActive;

	enum { kNone, kRotating, kDragging } fTracking;
	BPoint fLast;
	float fLastPitch, fLastYaw, fPrevPitch, fPrevYaw;
	bool fMoved;
	bool fDepthOnly;
};

#endif
