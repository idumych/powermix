// App.cpp: PowerMix, a clone of BeOS R5's 3dmiX (TSoundApplication).
#if __has_include(<AboutWindow.h>)
#	include <AboutWindow.h>
#else
	// Haiku R1/beta6 has BAboutWindow in libbe with a private header.
#	include <private/interface/AboutWindow.h>
#endif
#include <Application.h>
#include <Catalog.h>
#include <Entry.h>
#include <Path.h>
#include <Screen.h>

#include "AudioEngine.h"
#include "MainWindow.h"
#include "PowerMix.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "App"

class App : public BApplication {
public:
	App();
	virtual ~App();

	virtual void ReadyToRun();
	virtual void ArgvReceived(int32 argc, char** argv);
	virtual void RefsReceived(BMessage* message);
	virtual void AboutRequested();

private:
	AudioEngine fEngine;
	MainWindow* fWindow;
};

App::App()
	:
	BApplication(APP_SIGNATURE)
{
	// TSoundApplication: BRect(30, 30, 730, 430)
	fWindow = new MainWindow(BRect(30, 30, 730, 430), &fEngine);
	fWindow->Show();
}

App::~App()
{
	fEngine.Stop();
}

void
App::ReadyToRun()
{
	fEngine.Start();
	fWindow->ShowControls();
}

// ArgvReceived/RefsReceived → TWaveWindow::TakeRef. Unlike 3dmiX, links
// are followed (3dmiX opened the R5 samples' links as empty projects).
void
App::ArgvReceived(int32 argc, char** argv)
{
	BMessage* message = CurrentMessage();
	const char* cwd = NULL;
	if (message == NULL || message->FindString("cwd", &cwd) != B_OK)
		cwd = NULL;
	for (int32 i = 1; i < argc; i++) {
		BPath path;
		if (argv[i][0] == '/' || cwd == NULL)
			path.SetTo(argv[i]);
		else
			path.SetTo(cwd, argv[i]);
		BEntry entry(path.Path(), true);
		BPath resolved;
		if (entry.GetPath(&resolved) == B_OK && fWindow->Lock()) {
			fWindow->OpenProject(resolved.Path());
			fWindow->Unlock();
		}
	}
}

void
App::RefsReceived(BMessage* message)
{
	message->what = B_REFS_RECEIVED;
	fWindow->PostMessage(message);
}

void
App::AboutRequested()
{
	BAboutWindow* about = new BAboutWindow("PowerMix", APP_SIGNATURE);
	about->AddDescription(B_TRANSLATE(
		"A 3D sound mixer in the spirit of 3dmiX, the mixer that came with BeOS 5."));
	about->AddCopyright(2026, "the PowerMix authors");
	about->Show();
}

int
main()
{
	App app;
	app.Run();
	return 0;
}
