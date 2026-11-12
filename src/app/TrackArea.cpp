// TrackArea.cpp: first version, track names only.
#include "TrackArea.h"

#include <String.h>

#include "AudioEngine.h"

TrackArea::TrackArea(BRect frame, AudioEngine* engine)
	:
	BView(frame, "tracks", B_FOLLOW_ALL_SIDES, B_WILL_DRAW),
	fEngine(engine),
	fTime(0)
{
	SetViewColor(200, 200, 200);
}

void
TrackArea::Draw(BRect updateRect)
{
	if (!fEngine->Lock())
		return;
	float y = 60;
	for (size_t i = 0; i < fEngine->Project().tracks.size(); i++, y += 68) {
		BString name;
		name << int32(i) << "-" << fEngine->Project().tracks[i].name.c_str();
		SetHighColor(0, 102, 152);
		DrawString(name.String(), BPoint(12, y));
	}
	fEngine->Unlock();
}

void
TrackArea::ProjectChanged()
{
	Invalidate();
}

void
TrackArea::SetTime(double time)
{
	fTime = time;
}

void
TrackArea::SetSelection(const BMessage& tracks)
{
}
