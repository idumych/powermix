// MainWindow.h: PowerMix's one window (3dmiX's TWaveWindow).
#ifndef POWERMIX_MAIN_WINDOW_H
#define POWERMIX_MAIN_WINDOW_H

#include <String.h>
#include <Window.h>

class AudioEngine;
class BFilePanel;
class BMenuItem;
class BMessageRunner;
class MixerView;
namespace pmx { class Editor; }
class StatusView;
class TrackArea;

class MainWindow : public BWindow {
public:
	MainWindow(BRect frame, AudioEngine* engine);
	virtual ~MainWindow();

	virtual void MessageReceived(BMessage* message);
	virtual bool QuitRequested();
	virtual void MenusBeginning();

	status_t OpenProject(const char* path);
	// The controls slide in at start (TSoundApplication calls Switch_H)
	void ShowControls();

private:
	void BuildMenus(BRect& menuFrame);
	void SwitchMixer();		// TWaveWindow::Switch
	void SwitchControls();	// TWaveWindow::Switch_H
	void Animate();
	status_t SaveProject(const char* folder, const char* name);
	status_t SaveMix(const char* path);
	void ShowError(const char* text);
	void ProjectStructureChanged(bool keepSelection = false);
	void DropRefs(BMessage* message);

	AudioEngine* fEngine;
	BView* fOuter;		// 0x180: the strip and the inner view
	BView* fInner;		// 0x17c: the 3D view above the track area
	MixerView* fMixer;
	TrackArea* fTrackArea;
	StatusView* fStatus;
	BMessageRunner* fRunner;
	BFilePanel* fOpenPanel;
	BFilePanel* fSavePanel;
	BFilePanel* fWavePanel;
	BMenuItem* fSaveItem;
	BMenuItem* fSaveAsItem;
	BMenuItem* fSaveWaveItem;
	BMenuItem* fPauseItem;
	BMenuItem* fFastItem;
	BMenuItem* fUndoItem;
	BMenuItem* fClearItem;
	BMenuItem* fCutItem;
	BMenuItem* fCopyItem;
	BMenuItem* fPasteItem;
	pmx::Editor* fEditor;
	bool fMixerShown;		// 0x190
	bool fControlsShown;	// 0x194
	BString fProjectFolder;
	BString fProjectName;
};

#endif
