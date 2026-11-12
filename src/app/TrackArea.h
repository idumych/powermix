// TrackArea.h: the lower half of the window: the Ticks control, the time
// ruler and the track lanes (3dmiX's TCtrlView, TimeView, TrackView).
#ifndef POWERMIX_TRACK_AREA_H
#define POWERMIX_TRACK_AREA_H

#include <View.h>

class AudioEngine;

class TrackArea : public BView {
public:
	TrackArea(BRect frame, AudioEngine* engine);

	virtual void Draw(BRect updateRect);

	// The project changed (loaded, tracks added or removed)
	void ProjectChanged();
	void SetTime(double time);
	void SetSelection(const BMessage& tracks);

private:
	AudioEngine* fEngine;
	double fTime;
};

#endif
