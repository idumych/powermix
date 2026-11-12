// AboutWindow.h: PowerMix's About window, in the manner of 3dmiX's
// (TAboutWindow, TAboutView): the name forms out of a swirl of dots while
// a chord swells, then the credits scroll; a click scatters it and closes.
#ifndef POWERMIX_ABOUT_WINDOW_H
#define POWERMIX_ABOUT_WINDOW_H

#include <Window.h>

class AudioEngine;

class PowerMixAboutWindow : public BWindow {
public:
	PowerMixAboutWindow(AudioEngine* engine);

	virtual bool QuitRequested();

private:
	AudioEngine* fEngine;
};

#endif
