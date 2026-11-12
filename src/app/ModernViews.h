// ModernViews.h: the parts of the optional modern look (Display ▸ Modern
// look) that the classic window doesn't have: a toolbar, a track list on
// the left and an inspector for the selected track on the right. The 3D
// view and the track lanes are the classic window's own, drawn darker.
#ifndef POWERMIX_MODERN_VIEWS_H
#define POWERMIX_MODERN_VIEWS_H

#include <Control.h>
#include <String.h>
#include <View.h>

class AudioEngine;
class BBitmap;
class MixerView;

// Recording, as the window keeps it and the views show it
struct RecordState {
	int armed = -1;			// the track a take goes into; -1: a new track
	bool recording = false;
	bool countingIn = false;	// the bar of clicks before the take
	float level = 0;		// the input's peak, 0..1
};

// A flat button with an icon and/or a label. A toggle shows its state
// with SetValue(B_CONTROL_ON).
class ModernButton : public BControl {
public:
	enum Icon {
		kNoIcon, kIconStart, kIconPlay, kIconPause, kIconPlus, kIconMinus, kIconExport,
		kIconReset, kIconChevron, kIconRecord
	};
	enum Kind {
		kPlain,		// raised, outlined
		kPrimary,	// accent-filled; round when square
		kSegment,	// one part of a segmented switch
		kOverlay,	// on top of the 3D view
		kRecord		// round, red while it's on
	};

	ModernButton(BRect frame, const char* name, const char* label, BMessage* message,
		Icon icon = kNoIcon, Kind kind = kPlain,
		uint32 resizing = B_FOLLOW_LEFT | B_FOLLOW_TOP);

	void SetGlyph(Icon icon);
	void SetBackground(rgb_color color);

	virtual void Draw(BRect updateRect);
	virtual void MouseDown(BPoint where);
	virtual void MouseMoved(BPoint where, uint32 transit, const BMessage* drag);
	virtual void MouseUp(BPoint where);

private:
	Icon fIcon;
	Kind fKind;
	rgb_color fBackground;
	bool fTracking;
	bool fInside;
};

// Draws into an offscreen bitmap first, so redrawing every animation
// frame doesn't flicker
class BufferedView : public BView {
public:
	BufferedView(BRect frame, const char* name, uint32 resizing);
	virtual ~BufferedView();

	virtual void Draw(BRect updateRect);
	virtual void FrameResized(float width, float height);

protected:
	virtual void DrawContent(BView* into, BRect bounds) = 0;

private:
	BBitmap* fBitmap;
	BView* fPen;
};

class MasterBar;
class ValueField;

// Across the top: the project, start and pause, the time, the beat grid,
// the Stage | Split | Timeline switch, the master volume, the meter and
// Save mix as WAVE
class ToolbarView : public BufferedView {
public:
	static constexpr float kHeight = 56;

	ToolbarView(BRect frame);

	virtual void AttachedToWindow();
	virtual void FrameResized(float width, float height);
	virtual void MessageReceived(BMessage* message);

	void SetProject(const char* name);
	void SetTime(double time, double length);
	void SetLevels(float left, float right);
	void SetPaused(bool paused);
	void SetViewMode(int mode);
	void SetTicks(float ticks);
	void SetBeats(int beats);
	void SetMaster(float value);
	void SetRecording(bool recording, bool countingIn, float level);

protected:
	virtual void DrawContent(BView* into, BRect bounds);

private:
	void LayoutChildren();
	float Left() const;
	float TicksLeft() const;
	float TimeLeft() const;
	bool Narrow() const;

	ModernButton* fStart;
	ModernButton* fPlay;
	ModernButton* fRecord;
	ModernButton* fBeats;
	ModernButton* fSegments[3];
	ModernButton* fExport;
	ValueField* fTicks;
	MasterBar* fMaster;
	BString fProject;
	double fTime, fLength;
	float fLeft, fRight;
	BRect fMeter;
	bool fRecording;
	bool fCountingIn;
};

// The tracks down the left: colour, name, gain, level, mute and solo
class TrackListView : public BufferedView {
public:
	static constexpr float kWidth = 232;

	TrackListView(BRect frame, AudioEngine* engine, MixerView* mixer,
		const RecordState* record);

	virtual void MouseDown(BPoint where);
	virtual void MessageReceived(BMessage* message);

protected:
	virtual void DrawContent(BView* into, BRect bounds);

private:
	BRect RowFrame(int index) const;
	BRect MuteFrame(BRect row) const;
	BRect SoloFrame(BRect row) const;
	BRect ArmFrame(BRect row) const;

	AudioEngine* fEngine;
	MixerView* fMixer;
	const RecordState* fRecord;
	float fScroll;
};

// The selected track on the right: its place on the floor seen from
// above (drag it there), what the mix makes of it, mute, solo, edit
class InspectorView : public BufferedView {
public:
	static constexpr float kWidth = 260;

	InspectorView(BRect frame, AudioEngine* engine, MixerView* mixer,
		const RecordState* record);

	virtual void FrameResized(float width, float height);
	virtual void MouseDown(BPoint where);
	virtual void MouseMoved(BPoint where, uint32 transit, const BMessage* drag);
	virtual void MouseUp(BPoint where);

	// Redraws when the selected track moved or changed
	void Refresh();

protected:
	virtual void DrawContent(BView* into, BRect bounds);

private:
	BRect PadFrame() const;
	BPoint PadPoint(float x, float y) const;
	void FloorPoint(BPoint where, float& x, float& y) const;
	int Selected() const;
	void LayoutChildren();

	AudioEngine* fEngine;
	MixerView* fMixer;
	ModernButton* fMute;
	ModernButton* fSolo;
	ModernButton* fArm;
	ModernButton* fEdit;
	const RecordState* fRecord;
	int fDragging;
	BString fSignature;
};

#endif
