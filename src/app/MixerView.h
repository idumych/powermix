// MixerView.h: the 3D mixer (TSoundView's view half): shows the core
// Scene, moves tracks and the camera with the mouse and keys.
#ifndef POWERMIX_MIXER_VIEW_H
#define POWERMIX_MIXER_VIEW_H

#include <View.h>

#include <utility>
#include <vector>

#include "Scene.h"

class AudioEngine;
namespace pmx { class Editor; }
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

	// Floor automation. With Record moves on, moving a track while the mix
	// plays writes its path; otherwise a track with a path follows it, and
	// moving the track moves the whole path.
	void SetEditor(pmx::Editor* editor) { fEditor = editor; }
	void SetRecordMoves(bool on) { fRecordMoves = on; }
	bool RecordMoves() const { return fRecordMoves; }
	// The inspector's floor plan moves a track through these
	void BeginMove(int track);
	void MoveTrack(int track, float x, float y);
	void EndMove();

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
	bool Touched(int track) const;
	void ShiftPaths(const std::vector<std::pair<float, float> >& before);
	void CommitForMove(bool selection, int track);

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
	pmx::Editor* fEditor;
	bool fRecordMoves;
	int fMoving;				// the inspector's track; -1: none
	bool fKeyMove;				// an arrow key is moving the selection
	float fLastHeard;			// the time WritePositions last saw
};

#endif
