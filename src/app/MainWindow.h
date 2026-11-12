// MainWindow.h: PowerMix's one window (3dmiX's TWaveWindow).
#ifndef POWERMIX_MAIN_WINDOW_H
#define POWERMIX_MAIN_WINDOW_H

#include <Messenger.h>
#include <String.h>

#include <set>
#include <vector>
#include <Window.h>

#include "Look.h"
#include "ModernViews.h"
#include "Recorder.h"

class AudioEngine;
class BFilePanel;
class InspectorView;
class ToolbarView;
class TrackListView;
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
	virtual void FrameResized(float width, float height);

	status_t OpenProject(const char* path);
	// The controls slide in at start (TSoundApplication calls Switch_H)
	void ShowControls();
	void RunScript(const char* script);

	// Display ▸ Modern look: the same views in a darker layout with a
	// toolbar, a track list and an inspector
	void SetModern(bool modern);

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
	void BuildModern();
	void LayoutModern();
	void ResizeKeepingPlace(BRect size);
	void SetViewMode(int mode);
	bool StageVisible() const;
	void SelectTracks(const std::set<int>& tracks);
	void SelectionChanged();
	void ProjectNameChanged();
	void SyncModernControls();
	// Tracks ▸ Record: a take from the play position into the armed track
	// (or a new one); it stops by itself where the loop starts over
	void StartRecording();
	void StopRecording();
	void SetArmed(int track);

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
	BMenuItem* fRedoItem;
	BMenuItem* fClearItem;
	BMenuItem* fCutItem;
	BMenuItem* fCopyItem;
	BMenuItem* fPasteItem;
	pmx::Editor* fEditor;
	std::vector<BMessenger> fEditors;
	bool fMixerShown;		// 0x190
	bool fControlsShown;	// 0x194
	BString fProjectFolder;
	BString fProjectName;
	float fMasterValue;		// the master slider's, 0 (full) to 100

	Settings fSettings;
	bool fModern;
	int fViewMode;			// 0 stage, 1 split, 2 timeline
	BMenuItem* fModernItem;
	BMenuItem* fControlsItem;
	BView* fModernRoot;
	ToolbarView* fToolbar;
	TrackListView* fTrackList;
	InspectorView* fInspector;
	BView* fStageFrame;
	BView* fTimelineFrame;
	Recorder fRecorder;
	RecordState fRecord;
	float fTakeStart;			// where the take began, seconds
	double fLastTime;			// the mix's time at the last frame
	BMenuItem* fRecordItem;
	BMenuItem* fArmItem;
	BRect fClassicTrackFrame;	// the track area's frame in the classic layout
	float fClassicZoom;
	float fClassicViewStart;
};

#endif
