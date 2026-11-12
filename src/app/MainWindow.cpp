// MainWindow.cpp: see MainWindow.h. Layout and animations follow 3dmiX's
// TWaveWindow constructor, Switch and Switch_H.
#include "MainWindow.h"

#include <Alert.h>
#include <Application.h>
#include <Catalog.h>
#include <Entry.h>
#include <FilePanel.h>
#include <Menu.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <MessageRunner.h>
#include <NodeInfo.h>
#include <Path.h>
#include <Screen.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>

#include "AudioEngine.h"
#include "BeatWindow.h"
#include "EditWindow.h"
#include "Edit.h"
#include "Importer.h"
#include "LatencyWindow.h"
#include "WaveReader.h"
#include "MixerView.h"
#include "ModernViews.h"
#include "PowerMix.h"
#include "StatusView.h"
#include "TrackArea.h"
#include "WaveFile.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "MainWindow"

static const float kMixerHeight = 480;	// the 3D view's height
static const float kStripWidth = 64;	// Switch_H slides this far

MainWindow::MainWindow(BRect frame, AudioEngine* engine)
	:
	BWindow(frame, "PowerMix", B_DOCUMENT_WINDOW, B_NOT_ZOOMABLE),
	fEngine(engine),
	fRunner(NULL),
	fOpenPanel(NULL),
	fSavePanel(NULL),
	fWavePanel(NULL),
	fMixerShown(false),
	fControlsShown(false),
	fMasterValue(50),
	fModern(false),
	fViewMode(1),
	fModernItem(NULL),
	fControlsItem(NULL),
	fModernRoot(NULL),
	fToolbar(NULL),
	fTrackList(NULL),
	fInspector(NULL),
	fStageFrame(NULL),
	fTimelineFrame(NULL),
	fTakeStart(0),
	fLastTime(0),
	fRecordItem(NULL),
	fArmItem(NULL),
	fClickItem(NULL),
	fCountInItem(NULL),
	fMovesItem(NULL),
	fClearMovesItem(NULL),
	fCountInSeconds(0),
	fClassicZoom(1.0f / 14),
	fClassicViewStart(0)
{
	fSettings.Load();
	fViewMode = fSettings.viewMode;
	fEditor = new pmx::Editor(&engine->Project());
	BRect menuFrame = Bounds();
	BuildMenus(menuFrame);
	const float menuHeight = menuFrame.Height();
	const float width = frame.Width(), height = frame.Height();

	fOuter = new BView(BRect(0, menuHeight + 1, 5000, height + kMixerHeight), "outer",
		B_FOLLOW_ALL_SIDES, B_WILL_DRAW);
	fInner = new BView(BRect(66, 0, 5000, height + kMixerHeight), "inner",
		B_FOLLOW_ALL_SIDES, B_WILL_DRAW);
	AddChild(fOuter);
	fOuter->AddChild(fInner);
	fInner->ScrollBy(0, kMixerHeight);
	fInner->SetViewColor(B_TRANSPARENT_COLOR);
	fOuter->SetViewColor(120, 120, 120);	// the gap beside the strip

	fMixer = new MixerView(BRect(0, 0, 1023, kMixerHeight), engine);
	fInner->AddChild(fMixer);

	fTrackArea = new TrackArea(BRect(0, kMixerHeight, width, height + 50 + kMixerHeight),
		engine);
	fInner->AddChild(fTrackArea);
	fMixer->SetSelectionTarget(this);
	fMixer->SetEditor(fEditor);
	fTrackArea->SetEditor(fEditor);

	fStatus = new StatusView(BRect(0, 0, 63, 32000), this);
	fOuter->AddChild(fStatus);
	fOuter->ScrollBy(66, 0);

	BMessage animate(kMsgAnimate);
	fRunner = new BMessageRunner(BMessenger(this), &animate, 35000);

	if (fSettings.modern)
		SetModern(true);
}

MainWindow::~MainWindow()
{
	delete fRunner;
	delete fOpenPanel;
	delete fSavePanel;
	delete fWavePanel;
	delete fEditor;
}

void
MainWindow::BuildMenus(BRect& menuFrame)
{
	BMenuBar* bar = new BMenuBar(menuFrame, "menu");

	BMenu* menu = new BMenu(B_TRANSLATE("File"));
	menu->AddItem(new BMenuItem(B_TRANSLATE("Open project" B_UTF8_ELLIPSIS),
		new BMessage(kMsgOpenProject), 'O'));
	menu->AddItem(fSaveItem = new BMenuItem(B_TRANSLATE("Save project"),
		new BMessage(kMsgSaveProject), 'S'));
	menu->AddItem(fSaveAsItem = new BMenuItem(B_TRANSLATE("Save project as" B_UTF8_ELLIPSIS),
		new BMessage(kMsgSaveProjectAs)));
	menu->AddSeparatorItem();
	menu->AddItem(fSaveWaveItem = new BMenuItem(B_TRANSLATE("Save mix as WAVE" B_UTF8_ELLIPSIS),
		new BMessage(kMsgSaveMixAsWave)));
	menu->AddSeparatorItem();
	menu->AddItem(new BMenuItem(B_TRANSLATE("About PowerMix" B_UTF8_ELLIPSIS),
		new BMessage(kMsgAbout)));
	menu->AddSeparatorItem();
	menu->AddItem(new BMenuItem(B_TRANSLATE("Quit"), new BMessage(kMsgQuit), 'Q'));
	bar->AddItem(menu);

	menu = new BMenu(B_TRANSLATE("Edit"));
	menu->AddItem(fUndoItem = new BMenuItem(B_TRANSLATE("Undo"), new BMessage(kMsgUndo), 'Z'));
	menu->AddItem(fRedoItem = new BMenuItem(B_TRANSLATE("Redo"), new BMessage(kMsgRedo), 'Z',
		B_SHIFT_KEY));
	menu->AddSeparatorItem();
	menu->AddItem(fClearItem = new BMenuItem(B_TRANSLATE("Clear"), new BMessage(kMsgClear)));
	menu->AddItem(fCutItem = new BMenuItem(B_TRANSLATE("Cut"), new BMessage(kMsgCut), 'X'));
	menu->AddItem(fCopyItem = new BMenuItem(B_TRANSLATE("Copy"), new BMessage(kMsgCopy), 'C'));
	menu->AddItem(fPasteItem = new BMenuItem(B_TRANSLATE("Paste"), new BMessage(kMsgPaste),
		'V'));
	bar->AddItem(menu);

	menu = new BMenu(B_TRANSLATE("Display"));
	menu->AddItem(new BMenuItem(B_TRANSLATE("Show/hide mixer"),
		new BMessage(kMsgShowHideMixer), 'T'));
	menu->AddItem(fControlsItem = new BMenuItem(B_TRANSLATE("Show/hide controls"),
		new BMessage(kMsgShowHideControls), 'U'));
	menu->AddItem(new BMenuItem(B_TRANSLATE("Show/hide beat marks"),
		new BMessage(kMsgShowHideBeatMarks), 'B'));
	menu->AddSeparatorItem();
	menu->AddItem(fFastItem = new BMenuItem(B_TRANSLATE("Fast display"),
		new BMessage(kMsgFastDisplay)));
	menu->AddSeparatorItem();
	menu->AddItem(fModernItem = new BMenuItem(B_TRANSLATE("Modern look"),
		new BMessage(kMsgModernLook)));
	bar->AddItem(menu);

	menu = new BMenu(B_TRANSLATE("Tracks"));
	menu->AddItem(new BMenuItem(B_TRANSLATE("(Un)mute selection"),
		new BMessage(kMsgMuteSelection), 'M'));
	menu->AddItem(new BMenuItem(B_TRANSLATE("(Un)solo selection"),
		new BMessage(kMsgSoloSelection), 'L'));
	menu->AddSeparatorItem();
	menu->AddItem(new BMenuItem(B_TRANSLATE("New track"), new BMessage(kMsgNewTrack)));
	menu->AddItem(new BMenuItem(B_TRANSLATE("Delete tracks"), new BMessage(kMsgDeleteTracks)));
	menu->AddItem(new BMenuItem(B_TRANSLATE("Select all tracks"),
		new BMessage(kMsgSelectAllTracks), 'A'));
	menu->AddSeparatorItem();
	menu->AddItem(fArmItem = new BMenuItem(B_TRANSLATE("Arm selected track for recording"),
		new BMessage(kMsgToggleArm)));
	menu->AddItem(fRecordItem = new BMenuItem(B_TRANSLATE("Record"), new BMessage(kMsgRecord),
		'R'));
	menu->AddItem(fClickItem = new BMenuItem(B_TRANSLATE("Click while recording"),
		new BMessage(kMsgClick)));
	menu->AddItem(fCountInItem = new BMenuItem(B_TRANSLATE("Count-in before recording"),
		new BMessage(kMsgCountIn)));
	menu->AddSeparatorItem();
	menu->AddItem(fMovesItem = new BMenuItem(B_TRANSLATE("Record moves"),
		new BMessage(kMsgRecordMoves)));
	menu->AddItem(fClearMovesItem = new BMenuItem(B_TRANSLATE("Clear moves of selected tracks"),
		new BMessage(kMsgClearMoves)));
	bar->AddItem(menu);

	menu = new BMenu(B_TRANSLATE("Misc"));
	menu->AddItem(new BMenuItem(B_TRANSLATE("Ticks from time selection"),
		new BMessage(kMsgTicksFromSelection)));
	menu->AddItem(new BMenuItem(B_TRANSLATE("Ticks from sample length"),
		new BMessage(kMsgTicksFromSample)));
	menu->AddItem(new BMenuItem(B_TRANSLATE("Recording latency" B_UTF8_ELLIPSIS),
		new BMessage(kMsgLatency)));
	menu->AddSeparatorItem();
	menu->AddItem(fPauseItem = new BMenuItem(B_TRANSLATE("Pause"), new BMessage(kMsgPause),
		'P'));
	bar->AddItem(menu);

	AddChild(bar);
	menuFrame = bar->Bounds();
}

void
MainWindow::MenusBeginning()
{
	bool haveProject = false;
	if (fEngine->Lock()) {
		haveProject = !fEngine->Project().tracks.empty();
		fEngine->Unlock();
	}
	fSaveItem->SetEnabled(haveProject && fProjectFolder.Length() > 0);
	fSaveAsItem->SetEnabled(haveProject);
	fSaveWaveItem->SetEnabled(haveProject);
	fPauseItem->SetMarked(fEngine->IsPaused());
	fUndoItem->SetEnabled(fEditor->CanUndo());
	fRedoItem->SetEnabled(fEditor->CanRedo());
	pmx::ClipSelection clips;
	size_t a, b;
	fTrackArea->GetEditTargets(clips, a, b);
	fCutItem->SetEnabled(!clips.empty());
	fCopyItem->SetEnabled(!clips.empty());
	fClearItem->SetEnabled(!clips.empty());
	fPasteItem->SetEnabled(fEditor->HasClipboard() && haveProject);
	fFastItem->SetMarked(fTrackArea->FastDisplay());
	fModernItem->SetMarked(fModern);
	fRecordItem->SetMarked(fRecord.recording);
	fRecordItem->SetEnabled(Recorder::Available());
	bool armedSelected = false;
	bool anySelected = false;
	for (int i = 0; i < fMixer->Scene().CountTracks(); i++) {
		if (fMixer->Scene().IsSelected(i)) {
			anySelected = true;
			armedSelected |= i == fRecord.armed;
		}
	}
	fArmItem->SetEnabled(anySelected && !fRecord.recording);
	fClickItem->SetMarked(fSettings.click);
	fMovesItem->SetMarked(fMixer->RecordMoves());
	bool selectedPath = false;
	if (fEngine->Lock()) {
		const pmx::Project& project = fEngine->Project();
		for (int i = 0; i < fMixer->Scene().CountTracks() && i < int(project.tracks.size()); i++)
			selectedPath |= fMixer->Scene().IsSelected(i) && !project.tracks[size_t(i)].path.empty();
		fEngine->Unlock();
	}
	fClearMovesItem->SetEnabled(selectedPath);
	fCountInItem->SetMarked(fSettings.countIn);
	fArmItem->SetMarked(armedSelected);
	fControlsItem->SetEnabled(!fModern);
}

// #pragma mark - The modern look

void
MainWindow::BuildModern()
{
	BRect bounds = Bounds();
	const float top = KeyMenuBar()->Frame().bottom + 1;
	fModernRoot = new BView(BRect(0, top, bounds.right, bounds.bottom), "modern",
		B_FOLLOW_ALL_SIDES, 0);
	fModernRoot->SetViewColor(look::kGround);
	AddChild(fModernRoot);
	const float w = fModernRoot->Bounds().Width(), h = fModernRoot->Bounds().Height();

	fToolbar = new ToolbarView(BRect(0, 0, w, ToolbarView::kHeight - 1));
	fModernRoot->AddChild(fToolbar);
	fTrackList = new TrackListView(BRect(0, ToolbarView::kHeight, TrackListView::kWidth - 1, h),
		fEngine, fMixer, &fRecord);
	fModernRoot->AddChild(fTrackList);
	fInspector = new InspectorView(BRect(w - InspectorView::kWidth + 1, ToolbarView::kHeight, w,
		h), fEngine, fMixer, &fRecord);
	fModernRoot->AddChild(fInspector);
	// the 3D view and the lanes sit in frames whose colour is their border
	fStageFrame = new BView(BRect(0, 0, 10, 10), "stage", B_FOLLOW_NONE, 0);
	fStageFrame->SetViewColor(look::kLine);
	fModernRoot->AddChild(fStageFrame);
	fTimelineFrame = new BView(BRect(0, 0, 10, 10), "timeline", B_FOLLOW_NONE, 0);
	fTimelineFrame->SetViewColor(look::kLine);
	fModernRoot->AddChild(fTimelineFrame);
	fModernRoot->Hide();
}

// Display ▸ Modern look. The 3D view and the track area move between the
// two layouts; each layout keeps its own window frame.
void
MainWindow::SetModern(bool modern)
{
	if (modern == fModern)
		return;
	DisableUpdates();
	if (modern) {
		fSettings.classicFrame = Frame();
		fClassicTrackFrame = fTrackArea->Frame();
		fClassicZoom = fTrackArea->Zoom();
		fClassicViewStart = fTrackArea->ViewStart();
		fInner->RemoveChild(fMixer);
		fInner->RemoveChild(fTrackArea);
		fOuter->Hide();
		if (fModernRoot == NULL)
			BuildModern();
		look::SetModern(true);
		fModern = true;
		SetSizeLimits(980, 32768, 600, 32768);
		ResizeKeepingPlace(fSettings.modernFrame);
		fStageFrame->AddChild(fMixer);
		fTimelineFrame->AddChild(fTrackArea);
		fModernRoot->Show();
		fMixer->SetModern(true);
		LayoutModern();
		fTrackArea->SetModern(true);
		fTrackArea->ZoomToFit();
		SyncModernControls();
		ProjectNameChanged();
		fInspector->Refresh();
	} else {
		fSettings.modernFrame = Frame();
		fStageFrame->RemoveChild(fMixer);
		fTimelineFrame->RemoveChild(fTrackArea);
		fModernRoot->Hide();
		look::SetModern(false);
		fModern = false;
		SetSizeLimits(0, 32768, 0, 32768);
		ResizeKeepingPlace(fSettings.classicFrame);
		fMixer->SetModern(false);
		fMixer->MoveTo(0, 0);
		fMixer->ResizeTo(1023, kMixerHeight);
		fTrackArea->MoveTo(fClassicTrackFrame.LeftTop());
		fTrackArea->ResizeTo(fClassicTrackFrame.Width(), fClassicTrackFrame.Height());
		fInner->AddChild(fMixer);
		fInner->AddChild(fTrackArea);
		fTrackArea->SetModern(false);
		fTrackArea->ScrollTime(fClassicViewStart);
		fTrackArea->SetZoom(fClassicZoom);
		fOuter->Show();
		fMixer->SetActive(fMixerShown);
	}
	fSettings.modern = fModern;
	EnableUpdates();
	if (fModern && StageVisible())
		fMixer->MakeFocus(true);
}

// #pragma mark - Recording

void
MainWindow::SetArmed(int track)
{
	if (fRecord.recording)
		return;
	fRecord.armed = track;
	fTrackArea->SetArmed(track);
	SelectionChanged();
}

void
MainWindow::StartRecording()
{
	if (fRecord.recording)
		return;
	if (!Recorder::Available()) {
		ShowError(B_TRANSLATE("Recording needs the Media Kit, which this system doesn't "
			"have."));
		return;
	}
	float start = 0;
	if (fEngine->Lock()) {
		start = float(fEngine->Mixer().Time());
		if (fRecord.armed >= int(fEngine->Project().tracks.size()))
			fRecord.armed = -1;
		fEngine->Unlock();
	}
	status_t status = fRecorder.Start();
	if (status != B_OK) {
		BString text;
		text.SetToFormat(B_TRANSLATE("PowerMix couldn't record: no audio input could be "
			"opened (%s)."), strerror(status));
		ShowError(text.String());
		return;
	}
	// you hear the mix while you record, with the click on the ruler's
	// beats; a count-in holds the mix for a bar of clicks first
	if (fEngine->IsPaused())
		fEngine->SetPaused(false);
	float ticks = 0;
	int beats = 4;
	if (fEngine->Lock()) {
		ticks = fEngine->Project().ticks;
		beats = std::max<int>(1, fEngine->Project().beatsPerBar);
		fEngine->Unlock();
	}
	const double beat = ticks > 0 ? 120.0 / ticks : 0.5;
	fEngine->SetClick(fSettings.click, beat, beats);
	fCountInSeconds = 0;
	if (fSettings.countIn) {
		fEngine->StartCountIn(beats);
		fCountInSeconds = float(beats * (beat > 0.05 ? beat : 0.5));
	}
	fTakeStart = start;
	fLastTime = start;
	fRecord.recording = true;
	SyncModernControls();
	SelectionChanged();
}

void
MainWindow::StopRecording()
{
	if (!fRecord.recording)
		return;
	std::vector<int16> samples = fRecorder.Stop();
	fEngine->SetClick(false, 0, 1);
	// what was recorded during the count-in isn't part of the take
	const size_t countIn = std::min(samples.size(), size_t(fCountInSeconds * pmx::kSampleRate));
	samples.erase(samples.begin(), samples.begin() + long(countIn));
	fRecord.recording = false;
	fRecord.countingIn = false;
	fRecord.level = 0;
	fTrackArea->ClearRecording();
	SyncModernControls();
	if (samples.size() < size_t(pmx::kSampleRate / 100) || !fEngine->Lock()) {
		SelectionChanged();
		return;
	}
	// latency compensation: the take was heard this much later than mixed
	float start = fTakeStart;
	pmx::AlignTake(start, samples, float(fEngine->OutputDelay()
		+ fSettings.inputLatency / 1000.0));
	fEditor->Commit();
	pmx::Project& project = fEngine->Project();
	size_t track;
	if (fRecord.armed >= 0 && size_t(fRecord.armed) < project.tracks.size())
		track = size_t(fRecord.armed);
	else {
		// no armed track: a new one in front of the listener
		pmx::Track take;
		take.name = pmx::UniqueTrackName(project, "take");
		take.x = 0;
		take.y = 100;
		take.addedByHand = true;
		project.tracks.push_back(take);
		track = project.tracks.size() - 1;
	}
	int number = 1;
	for (const pmx::Track& t : project.tracks)
		for (const pmx::Clip& c : t.clips)
			number += c.source.compare(0, 4, "take") == 0;
	BString source;
	source.SetToFormat("take %d", number);
	pmx::PlaceRecording(project.tracks[track], start, pmx::MakeClip(source.String(),
		std::move(samples)));
	fEngine->Unlock();
	ProjectStructureChanged(true);
}

// The other look's size, here; moved only as far as needed to stay on
// screen
void
MainWindow::ResizeKeepingPlace(BRect size)
{
	BRect screen = BScreen(this).Frame();
	BRect frame = Frame();
	float width = std::min(size.Width(), screen.Width() - 20);
	float height = std::min(size.Height(), screen.Height() - 40);
	float left = std::max(screen.left + 4, std::min(frame.left, screen.right - 4 - width));
	float top = std::max(screen.top + 24, std::min(frame.top, screen.bottom - 4 - height));
	if (left != frame.left || top != frame.top)
		MoveTo(left, top);
	ResizeTo(width, height);
}

bool
MainWindow::StageVisible() const
{
	return fModern ? fViewMode != 2 : fMixerShown;
}

void
MainWindow::FrameResized(float width, float height)
{
	BWindow::FrameResized(width, height);
	if (fModern)
		LayoutModern();
}

void
MainWindow::LayoutModern()
{
	if (fModernRoot == NULL)
		return;
	BRect b = fModernRoot->Bounds();
	const float gap = 12;
	const float left = TrackListView::kWidth + gap;
	const float right = b.right - InspectorView::kWidth - gap;
	const float top = ToolbarView::kHeight + gap;
	const float bottom = b.bottom - gap;
	float stageBottom = top - 1, timelineTop = top;
	if (fViewMode == 0)
		stageBottom = bottom;
	else if (fViewMode == 1) {
		// as tall as the scaled frame needs, up to half
		float height = std::min((bottom - top - gap) * 0.5f,
			std::max((bottom - top - gap) * 0.36f, (right - left) * 480 / 1024 + 4));
		stageBottom = floorf(top + height);
		timelineTop = stageBottom + gap + 1;
	}
	if (fViewMode == 2) {
		if (!fStageFrame->IsHidden())
			fStageFrame->Hide();
	} else {
		fStageFrame->MoveTo(left, top);
		fStageFrame->ResizeTo(right - left, stageBottom - top);
		fMixer->MoveTo(1, 1);
		fMixer->ResizeTo(right - left - 2, stageBottom - top - 2);
		if (fStageFrame->IsHidden())
			fStageFrame->Show();
	}
	if (fViewMode == 0) {
		if (!fTimelineFrame->IsHidden())
			fTimelineFrame->Hide();
	} else {
		fTimelineFrame->MoveTo(left, timelineTop);
		fTimelineFrame->ResizeTo(right - left, bottom - timelineTop);
		fTrackArea->MoveTo(1, 1);
		fTrackArea->ResizeTo(right - left - 2, bottom - timelineTop - 2);
		if (fTimelineFrame->IsHidden())
			fTimelineFrame->Show();
	}
	fMixer->SetActive(StageVisible());
}

void
MainWindow::SetViewMode(int mode)
{
	if (mode < 0 || mode > 2)
		return;
	fViewMode = mode;
	fSettings.viewMode = mode;
	if (fToolbar != NULL)
		fToolbar->SetViewMode(mode);
	if (fModern) {
		LayoutModern();
		// the lanes keep their selection; the track area relays its children
		fTrackArea->SetModern(true);
		if (StageVisible())
			fMixer->MakeFocus(true);
	}
}

void
MainWindow::SyncModernControls()
{
	if (fToolbar == NULL)
		return;
	float ticks = 0;
	int beats = 4;
	if (fEngine->Lock()) {
		ticks = fEngine->Project().ticks;
		beats = fEngine->Project().beatsPerBar;
		fEngine->Unlock();
	}
	fToolbar->SetTicks(ticks);
	fToolbar->SetBeats(beats);
	fToolbar->SetMaster(fMasterValue);
	fToolbar->SetPaused(fEngine->IsPaused());
	fToolbar->SetViewMode(fViewMode);
}

void
MainWindow::ProjectNameChanged()
{
	if (fToolbar != NULL)
		fToolbar->SetProject(fProjectName.String());
}

// One selection for the 3D view, the lanes, the list and the inspector
void
MainWindow::SelectTracks(const std::set<int>& tracks)
{
	pmx::Scene& scene = fMixer->Scene();
	scene.DeselectAll();
	BMessage selection(kMsgMixerSelection);
	for (int track : tracks) {
		if (track >= 0 && track < scene.CountTracks()) {
			scene.Select(track, true);
			selection.AddInt32("track", track);
		}
	}
	fTrackArea->SetSelection(selection);
	fMixer->Redraw();
	SelectionChanged();
}

void
MainWindow::SelectionChanged()
{
	if (!fModern)
		return;
	fTrackList->Invalidate();
	fInspector->Refresh();
}

// TWaveWindow::Switch: slides the 3D view down over the tracks (or back
// up), speeding up by 10% a frame, 30% when frames are slow
void
MainWindow::SwitchMixer()
{
	float step = 1;
	int32 moved = 0;
	const float direction = fMixerShown ? 1 : -1;
	if (fMixerShown)
		fInner->ResizeBy(0, kMixerHeight);
	for (;;) {
		bigtime_t start = system_time();
		fInner->ScrollBy(0, direction * int32(step));
		UpdateIfNeeded();
		fInner->Sync();
		float factor = (system_time() - start) / 1000.0f <= 15.0f ? 1.1f : 1.3f;
		moved += int32(step);
		step *= factor;
		if (!(step + moved < kMixerHeight))
			break;
	}
	fInner->ScrollBy(0, direction * (kMixerHeight - moved));
	UpdateIfNeeded();
	if (!fMixerShown)
		fInner->ResizeBy(0, -kMixerHeight);
	fMixerShown = !fMixerShown;
	fMixer->SetActive(fMixerShown);
	if (fMixerShown)
		fMixer->MakeFocus(true);
}

// TWaveWindow::Switch_H: slides the meter strip in or out
void
MainWindow::SwitchControls()
{
	float step = 1;
	int32 moved = 0;
	if (fControlsShown)
		fOuter->ResizeBy(kStripWidth, 0);
	const float direction = fControlsShown ? 1 : -1;
	float angle = fControlsShown ? 0 : 3.1415925f;
	const float turn = fControlsShown ? 0.07853981f : -0.07853981f;
	do {
		bigtime_t start = system_time();
		angle += turn;
		fTrackArea->SetControlsRotation(angle);
		fOuter->ScrollBy(direction * int32(step), 0);
		UpdateIfNeeded();
		fOuter->Sync();
		float factor = (system_time() - start) / 1000.0f <= 15.0f ? 1.1f : 1.3f;
		moved += int32(step);
		step *= factor;
	} while (moved < kStripWidth);
	fOuter->ScrollBy(direction * (kStripWidth - moved), 0);
	if (!fControlsShown)
		fOuter->ResizeBy(-kStripWidth, 0);
	UpdateIfNeeded();
	fControlsShown = !fControlsShown;
	// then the triangle finishes turning, faster while the mixer shows
	const float target = fControlsShown ? 0 : 3.1415925f;
	const float rest = (fMixerShown ? 0.15707962f : 0.07853981f) * (fControlsShown ? -1 : 1);
	while (fControlsShown ? angle > target : angle < target) {
		angle += rest;
		fTrackArea->SetControlsRotation(angle);
		UpdateIfNeeded();
		snooze(10000);
	}
	fTrackArea->SetControlsRotation(target);
}

void
MainWindow::ShowControls()
{
	if (Lock()) {
		if (!fControlsShown && !fModern)
			SwitchControls();
		Unlock();
	}
}

void
MainWindow::Animate()
{
	fMixer->Animate(StageVisible());
	// the strip draws at once, hidden or not
	if (!fModern)
		fStatus->SetLevel(fEngine->LeftLevel(), fEngine->RightLevel());
	double time = 0, length = 0;
	if (fEngine->Lock()) {
		time = fEngine->Mixer().Time();
		length = fEngine->Project().Length();
		fEngine->Unlock();
	}
	fTrackArea->SetTime(time);
	if (fRecord.recording) {
		// the take stops where the loop starts over
		if (time + 0.05 < fLastTime)
			StopRecording();
		else {
			fRecord.level = fRecorder.Level();
			int track = fRecord.armed >= 0 ? fRecord.armed : -2;
			if (track == -2 && fEngine->Lock()) {
				track = int(fEngine->Project().tracks.size());
				fEngine->Unlock();
			}
			fRecord.countingIn = fEngine->CountingIn();
			fTrackArea->SetRecording(track, fTakeStart, fTakeStart
				+ std::max(0.0, fRecorder.Seconds() - fCountInSeconds));
		}
	}
	fLastTime = time;
	if (fModern) {
		fToolbar->SetRecording(fRecord.recording, fRecord.countingIn, fRecord.level);
		fToolbar->SetTime(time, length);
		fToolbar->SetLevels(fEngine->LeftLevel(), fEngine->RightLevel());
		fTrackList->Invalidate();
		fInspector->Refresh();
	}
}

status_t
MainWindow::OpenProject(const char* path)
{
	pmx::Project project;
	std::string error;
	std::vector<std::string> skipped;
	if (!pmx::ReadProject(path, project, &error, &skipped)) {
		ShowError(error.c_str());
		return B_ERROR;
	}
	if (!fEngine->Lock())
		return B_ERROR;
	fEngine->Project() = project;
	fEditor->Reset();
	pmx::Mixer& mixer = fEngine->Mixer();
	mixer.Reset();
	mixer.SetRange(project.selectionStart, project.selectionEnd);
	mixer.Seek(project.selectionStart);
	fMixer->SyncTracks();
	fEngine->Unlock();

	BPath file(path);
	BPath folder;
	file.GetParent(&folder);
	fProjectFolder = folder.Path();
	fProjectName = file.Leaf();
	fTrackArea->ProjectChanged();
	if (fModern)
		fTrackArea->ZoomToFit();
	ProjectNameChanged();
	SyncModernControls();
	SelectionChanged();
	if (!skipped.empty()) {
		BString text(B_TRANSLATE("These tracks couldn't be read:"));
		for (const std::string& name : skipped)
			text << "\n" << name.c_str();
		ShowError(text.String());
	}
	return B_OK;
}

status_t
MainWindow::SaveProject(const char* folder, const char* name)
{
	std::string error;
	bool ok = false;
	if (fEngine->Lock()) {
		ok = pmx::WriteProject(folder, name, fEngine->Project(), &error);
		fEngine->Unlock();
	}
	if (!ok) {
		ShowError(error.c_str());
		return B_ERROR;
	}
	BPath path(folder, name);
	BNode node(path.Path());
	BNodeInfo info(&node);
	info.SetType(PROJECT_TYPE);
	fProjectFolder = folder;
	fProjectName = name;
	ProjectNameChanged();
	return B_OK;
}

// TWaveWindow::MixMaster / final_mix: the selection mixed once into a file
status_t
MainWindow::SaveMix(const char* path)
{
	if (!fEngine->Lock())
		return B_ERROR;
	pmx::Project project = fEngine->Project();
	pmx::Mixer live = fEngine->Mixer();
	fEngine->Unlock();

	pmx::Mixer mixer(&project);
	for (size_t i = 0; i < project.tracks.size(); i++) {
		mixer.SetMuted(i, live.IsMuted(i));
		mixer.SetSoloed(i, live.IsSoloed(i));
	}
	mixer.SetMasterGain(pmx::MasterGainForSlider(fMasterValue));
	mixer.SetRange(project.selectionStart, project.selectionEnd);
	mixer.Seek(project.selectionStart);
	mixer.SetLoop(false);
	std::vector<int16> frames;
	int16 block[2 * pmx::kBlockFrames];
	while (mixer.NextBlock(block))
		frames.insert(frames.end(), block, block + 2 * pmx::kBlockFrames);
	if (WriteWave(path, frames) != B_OK) {
		ShowError(B_TRANSLATE("The mix couldn't be saved."));
		return B_ERROR;
	}
	return B_OK;
}

// TrackView::do_handle_drop: a project opens, a track file becomes a
// track, any other sound becomes a clip in the lane it was dropped on or a
// new track named after it
void
MainWindow::DropRefs(BMessage* message)
{
	int32 lane = -1;
	float time = 0, x = 0, y = 100;
	message->FindInt32("lane", &lane);
	message->FindFloat("time", &time);
	message->FindFloat("x", &x);
	message->FindFloat("y", &y);
	bool committed = false;
	bool changed = false;
	entry_ref ref;
	// refs, or paths as strings (from scripts: hey sends "refs" that way)
	const char* string;
	for (int32 i = 0; message->FindRef("refs", i, &ref) == B_OK
			|| (message->FindString("refs", i, &string) == B_OK
				&& get_ref_for_path(string, &ref) == B_OK); i++) {
		BEntry entry(&ref, true);
		BPath path;
		if (entry.GetPath(&path) != B_OK || entry.IsDirectory())
			continue;
		char magic[4] = {0, 0, 0, 0};
		FILE* file = fopen(path.Path(), "rb");
		if (file != NULL) {
			if (fread(magic, 1, 4, file) != 4)
				magic[0] = 0;
			fclose(file);
		}
		if (memcmp(magic, "MAST", 4) == 0) {
			OpenProject(path.Path());
			return;
		}
		pmx::Track track;
		bool isTrack = memcmp(magic, "!TRK", 4) == 0;
		std::vector<int16> samples;
		if (isTrack) {
			std::string error;
			if (!pmx::ReadTrack(path.Path(), track, &error))
				continue;
		} else if (DecodeSound(ref, samples) != B_OK) {
			BString text;
			text.SetToFormat(B_TRANSLATE("%s isn't a sound PowerMix can read."), path.Leaf());
			ShowError(text.String());
			continue;
		}
		if (!fEngine->Lock())
			continue;
		if (!committed) {
			fEditor->Commit();
			committed = true;
		}
		pmx::Project& project = fEngine->Project();
		if (!isTrack && lane >= 0 && size_t(lane) < project.tracks.size()) {
			// TrackViewer::HandleDrop: after the boundary nearest the drop
			pmx::Track& target = project.tracks[size_t(lane)];
			size_t index = 0;
			float nearest = 1e10f;
			for (size_t k = 0; k < target.clips.size(); k++) {
				if (fabsf(target.clips[k].start - time) <= nearest) {
					nearest = fabsf(target.clips[k].start - time);
					index = k;
				}
				if (fabsf(target.clips[k].end - time) <= nearest) {
					nearest = fabsf(target.clips[k].end - time);
					index = k + 1;
				}
			}
			pmx::InsertClip(target, index, pmx::MakeClip(path.Leaf(), samples));
		} else {
			if (!isTrack) {
				BString name(path.Leaf());
				int32 dot = name.FindLast('.');
				if (dot > 0)
					name.Truncate(dot);
				track.name = name.String();
				track.clips.push_back(pmx::MakeClip(path.Leaf(), samples));
			}
			track.name = pmx::UniqueTrackName(project, track.name);
			track.x = x;
			track.y = y;
			track.addedByHand = true;
			project.tracks.push_back(track);
			x = std::min(250.0f, x + 30);
		}
		fEngine->Unlock();
		changed = true;
	}
	if (changed)
		ProjectStructureChanged(true);
}

// --do 'open PATH; dump; export PATH; drop PATH LANE TIME; quit': for
// tests. Results go to stdout as key=value lines.
void
MainWindow::RunScript(const char* script)
{
	BString all(script);
	int32 start = 0;
	while (start <= all.Length()) {
		int32 end = all.FindFirst(';', start);
		if (end < 0)
			end = all.Length();
		BString command;
		all.CopyInto(command, start, end - start);
		command.Trim();
		start = end + 1;
		if (command.Length() == 0)
			continue;
		BString verb = command;
		int32 space = command.FindFirst(' ');
		BString rest;
		if (space > 0) {
			verb.Truncate(space);
			command.CopyInto(rest, space + 1, command.Length() - space - 1);
			rest.Trim();
		}
		if (verb == "open") {
			printf("open %s\n", OpenProject(rest.String()) == B_OK ? "ok" : "failed");
		} else if (verb == "export") {
			status_t status = SaveMix(rest.String());
			pmx::DecodedSound sound;
			if (status == B_OK && pmx::ReadWave(rest.String(), sound)) {
				double energy = 0;
				for (float v : sound.samples)
					energy += double(v) * v * 32768.0 * 32768.0;
				printf("export frames=%zu rms=%.0f\n", sound.Frames(),
					sqrt(energy / std::max<size_t>(1, sound.samples.size())));
			} else
				printf("export failed\n");
		} else if (verb == "drop") {
			// drop PATH LANE TIME
			BString path = rest, lane = "-1", time = "0";
			int32 a = rest.FindLast(' ');
			if (a > 0) {
				rest.CopyInto(time, a + 1, rest.Length() - a - 1);
				path.Truncate(a);
				int32 b = path.FindLast(' ');
				if (b > 0) {
					path.CopyInto(lane, b + 1, path.Length() - b - 1);
					path.Truncate(b);
				}
			}
			BMessage drop(kMsgDropRefs);
			drop.AddString("refs", path.String());
			drop.AddInt32("lane", atoi(lane.String()));
			drop.AddFloat("time", float(atof(time.String())));
			DropRefs(&drop);
			printf("drop done\n");
		} else if (verb == "dump") {
			if (fEngine->Lock()) {
				const pmx::Project& project = fEngine->Project();
				printf("tracks=%zu length=%.2f\n", project.tracks.size(), project.Length());
				for (size_t i = 0; i < project.tracks.size(); i++) {
					BString fades;
					for (size_t k = 0; k < project.tracks[i].clips.size(); k++) {
						const pmx::Clip& c = project.tracks[i].clips[k];
						if (c.fadeIn > 0 || c.fadeOut > 0)
							fades << " fade" << int32(k) << "=" << c.fadeIn << "/" << c.fadeOut;
					}
					if (!project.tracks[i].path.empty())
						fades << " path=" << int32(project.tracks[i].path.size());
					printf("track %zu name=%s clips=%zu x=%.1f y=%.1f%s\n", i,
						project.tracks[i].name.c_str(), project.tracks[i].clips.size(),
						project.tracks[i].x, project.tracks[i].y, fades.String());
				}
				fEngine->Unlock();
			}
		} else if (verb == "look") {
			// look modern|classic [stage|split|timeline]
			SetModern(rest.StartsWith("modern"));
			const char* modes[3] = {"stage", "split", "timeline"};
			for (int i = 0; i < 3; i++) {
				if (rest.FindFirst(modes[i]) >= 0)
					SetViewMode(i);
			}
			printf("look %s\n", fModern ? "modern" : "classic");
		} else if (verb == "undo" || verb == "redo") {
			BMessage step(verb == "undo" ? kMsgUndo : kMsgRedo);
			MessageReceived(&step);
			printf("%s steps=%zu\n", verb.String(), fEditor->UndoSteps());
		} else if (verb == "moves") {
			// moves on|off: Record moves
			fMixer->SetRecordMoves(rest == "on");
			printf("moves %s\n", rest == "on" ? "on" : "off");
		} else if (verb == "move") {
			// move TRACK TIME X Y: a point of the track's path
			int track = 0;
			float t = 0, x = 0, y = 0;
			sscanf(rest.String(), "%d %f %f %f", &track, &t, &x, &y);
			if (fEngine->Lock()) {
				pmx::Project& project = fEngine->Project();
				if (size_t(track) < project.tracks.size())
					pmx::WritePathPoint(project.tracks[track], t - 0.0001f, t, x, y);
				fEngine->Unlock();
			}
			printf("move done\n");
		} else if (verb == "wait") {
			snooze(bigtime_t(atof(rest.String()) * 1000000));
			printf("wait done\n");
		} else if (verb == "position") {
			// position TRACK: where the mix has it now, and the 3D view
			int track = atoi(rest.String());
			fMixer->Animate(false);
			if (fEngine->Lock()) {
				const pmx::Project& project = fEngine->Project();
				float px = 0, py = 0, sx = 0, sy = 0;
				double now = std::max(0.0, fEngine->Mixer().Time() - fEngine->OutputDelay());
				if (size_t(track) < project.tracks.size())
					project.tracks[track].PositionAt(now, px, py);
				fEngine->Unlock();
				if (track < fMixer->Scene().CountTracks())
					fMixer->Scene().GetPosition(track, sx, sy);
				printf("position track=%d time=%.2f path=%.1f,%.1f view=%.1f,%.1f\n", track, now,
					px, py, sx, sy);
			}
		} else if (verb == "click" || verb == "countin") {
			// click on|off, countin on|off
			(verb == "click" ? fSettings.click : fSettings.countIn) = rest == "on";
			printf("%s %s\n", verb.String(), rest == "on" ? "on" : "off");
		} else if (verb == "fade") {
			// fade TRACK CLIP IN OUT
			int track = 0, clip = 0;
			float in = 0, out = 0;
			sscanf(rest.String(), "%d %d %f %f", &track, &clip, &in, &out);
			if (fEngine->Lock()) {
				pmx::Project& project = fEngine->Project();
				if (size_t(track) < project.tracks.size()
					&& size_t(clip) < project.tracks[track].clips.size()) {
					fEditor->Commit();
					pmx::Clip& c = project.tracks[track].clips[clip];
					pmx::SetClipFades(c, in, out);
					printf("fade track=%d clip=%d in=%.2f out=%.2f\n", track, clip, c.fadeIn,
						c.fadeOut);
				} else
					printf("fade failed\n");
				fEngine->Unlock();
			}
		} else if (verb == "save") {
			// save PATH: the project there, as File > Save project as does
			BPath path(rest.String());
			BPath folder;
			path.GetParent(&folder);
			printf("save %s\n", SaveProject(folder.Path(), path.Leaf()) == B_OK ? "ok" : "failed");
		} else if (verb == "latency") {
			// latency MS: the extra input delay
			fSettings.inputLatency = atoi(rest.String());
			printf("latency output=%.1fms extra=%dms\n", fEngine->OutputDelay() * 1000,
				int(fSettings.inputLatency));
		} else if (verb == "arm") {
			SetArmed(atoi(rest.String()));
			printf("arm %d\n", fRecord.armed);
		} else if (verb == "record") {
			// record SECONDS: a take of that long from the play position
			StartRecording();
			if (!fRecord.recording) {
				printf("record failed\n");
				continue;
			}
			snooze(bigtime_t(atof(rest.String()) * 1000000));
			int target = fRecord.armed;
			StopRecording();
			if (fEngine->Lock()) {
				const pmx::Project& project = fEngine->Project();
				size_t t = target >= 0 ? size_t(target) : project.tracks.size() - 1;
				int peak = 0;
				float length = 0, start = 0;
				for (const pmx::Clip& c : project.tracks[t].clips) {
					if (c.source.compare(0, 4, "take") != 0)
						continue;
					length = pmx::ClipLength(c);
					start = c.start;
					peak = 0;
					for (int16 v : c.samples)
						peak = std::max(peak, abs(int(v)));
				}
				printf("record track=%zu name=%s clips=%zu take-start=%.2f take-length=%.2f "
					"peak=%d\n", t, project.tracks[t].name.c_str(), project.tracks[t].clips.size(),
					start, length, peak);
				fEngine->Unlock();
			}
		} else if (verb == "select") {
			std::set<int> tracks;
			tracks.insert(atoi(rest.String()));
			SelectTracks(tracks);
			printf("select done\n");
		} else if (verb == "quit") {
			fflush(stdout);
			be_app->PostMessage(B_QUIT_REQUESTED);
		} else
			printf("unknown command: %s\n", verb.String());
		fflush(stdout);
	}
}

// After tracks were added, deleted or undone: the mixer and both views
// start again from the project
void
MainWindow::ProjectStructureChanged(bool keepSelection)
{
	// rebuilding the 3D scene forgets its selection; keep it if asked
	std::set<int> selected;
	pmx::Scene& scene = fMixer->Scene();
	for (int i = 0; keepSelection && i < scene.CountTracks(); i++) {
		if (scene.IsSelected(i))
			selected.insert(i);
	}
	if (fEngine->Lock()) {
		fEngine->Mixer().Reset();
		fMixer->SyncTracks();
		fEngine->Unlock();
	}
	for (int i : selected) {
		if (i < scene.CountTracks())
			scene.Select(i, true);
	}
	fTrackArea->ProjectChanged(keepSelection);
	SelectionChanged();
}

void
MainWindow::ShowError(const char* text)
{
	BAlert* alert = new BAlert("PowerMix", text, B_TRANSLATE("OK"), NULL, NULL,
		B_WIDTH_AS_USUAL, B_WARNING_ALERT);
	alert->SetFlags(alert->Flags() | B_CLOSE_ON_ESCAPE);
	alert->Go(NULL);
}

void
MainWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgAnimate:
			Animate();
			break;

		case kMsgOpenProject:
			if (fOpenPanel == NULL) {
				fOpenPanel = new BFilePanel(B_OPEN_PANEL, new BMessenger(this), NULL,
					B_FILE_NODE, false, new BMessage(kMsgOpenPanelRefs));
			}
			fOpenPanel->Show();
			break;
		case kMsgOpenPanelRefs:
		case B_REFS_RECEIVED:
		{
			entry_ref ref;
			if (message->FindRef("refs", &ref) == B_OK) {
				BEntry entry(&ref, true);
				BPath path;
				if (entry.GetPath(&path) == B_OK)
					OpenProject(path.Path());
			}
			break;
		}
		case kMsgSaveProject:
			if (fProjectFolder.Length() > 0) {
				SaveProject(fProjectFolder, fProjectName);
				break;
			}
			// fall through
		case kMsgSaveProjectAs:
			if (fSavePanel == NULL) {
				fSavePanel = new BFilePanel(B_SAVE_PANEL, new BMessenger(this), NULL, 0,
					false, new BMessage(kMsgSavePanel));
			}
			fSavePanel->SetSaveText(fProjectName.Length() > 0 ? fProjectName.String()
				: "Untitled.3dmix");
			fSavePanel->Show();
			break;
		case kMsgSavePanel:
		{
			entry_ref directory;
			const char* name;
			if (message->FindRef("directory", &directory) == B_OK
				&& message->FindString("name", &name) == B_OK) {
				BPath path(&directory);
				SaveProject(path.Path(), name);
			}
			break;
		}
		case kMsgSaveMixAsWave:
			if (fWavePanel == NULL) {
				fWavePanel = new BFilePanel(B_SAVE_PANEL, new BMessenger(this), NULL, 0,
					false, new BMessage(kMsgSaveWavePanel));
			}
			fWavePanel->SetSaveText("mix.wav");
			fWavePanel->Show();
			break;
		case kMsgSaveWavePanel:
		{
			entry_ref directory;
			const char* name;
			if (message->FindRef("directory", &directory) == B_OK
				&& message->FindString("name", &name) == B_OK) {
				BPath path(&directory);
				path.Append(name);
				SaveMix(path.Path());
			}
			break;
		}
		case kMsgAbout:
			be_app->PostMessage(B_ABOUT_REQUESTED);
			break;
		case kMsgQuit:
			be_app->PostMessage(B_QUIT_REQUESTED);
			break;

		case kMsgShowHideMixer:
			// in the modern look: the stage with the lanes, or the lanes alone
			if (fModern)
				SetViewMode(fViewMode == 2 ? 1 : 2);
			else
				SwitchMixer();
			break;
		case kMsgShowHideControls:
			if (!fModern)
				SwitchControls();
			break;

		case kMsgModernLook:
			SetModern(!fModern);
			break;
		case kMsgViewMode:
		{
			int32 mode;
			if (message->FindInt32("mode", &mode) == B_OK)
				SetViewMode(mode);
			break;
		}
		case kMsgSeekStart:
			if (fEngine->Lock()) {
				fEngine->Mixer().Seek(fEngine->Project().selectionStart);
				fEngine->Unlock();
			}
			break;
		case kMsgSelectTrack:
		{
			int32 track;
			if (message->FindInt32("track", &track) != B_OK)
				break;
			std::set<int> tracks;
			if (message->GetBool("extend", false)) {
				for (int i = 0; i < fMixer->Scene().CountTracks(); i++) {
					if (fMixer->Scene().IsSelected(i))
						tracks.insert(i);
				}
				if (tracks.count(track) > 0)
					tracks.erase(track);
				else
					tracks.insert(track);
			} else
				tracks.insert(track);
			SelectTracks(tracks);
			break;
		}
		case kMsgToggleMute:
		case kMsgToggleSolo:
		{
			int32 track;
			if (message->FindInt32("track", &track) != B_OK || track < 0 || !fEngine->Lock())
				break;
			pmx::Mixer& mixer = fEngine->Mixer();
			if (size_t(track) < fEngine->Project().tracks.size()) {
				if (message->what == kMsgToggleMute)
					mixer.SetMuted(size_t(track), !mixer.IsMuted(size_t(track)));
				else
					mixer.SetSoloed(size_t(track), !mixer.IsSoloed(size_t(track)));
			}
			fEngine->Unlock();
			fTrackArea->RedrawLanes();
			SelectionChanged();
			break;
		}
		case kMsgSetBeats:
		{
			int32 beats;
			if (message->FindInt32("beats", &beats) == B_OK) {
				fTrackArea->SetBeats(beats);
				SyncModernControls();
			}
			break;
		}
		case kMsgRecord:
			if (fRecord.recording)
				StopRecording();
			else
				StartRecording();
			break;
		case kMsgRecordMoves:
			fMixer->SetRecordMoves(!fMixer->RecordMoves());
			SelectionChanged();
			break;
		case kMsgClearMoves:
		{
			// a track's, or the selected tracks'; they stay where they are now
			int32 only = -1;
			message->FindInt32("track", &only);
			if (!fEngine->Lock())
				break;
			pmx::Project& project = fEngine->Project();
			bool committed = false;
			for (int i = 0; i < fMixer->Scene().CountTracks() && i < int(project.tracks.size()); i++) {
				bool chosen = only >= 0 ? i == only : fMixer->Scene().IsSelected(i);
				pmx::Track& track = project.tracks[size_t(i)];
				if (!chosen || track.path.empty())
					continue;
				if (!committed) {
					fEditor->Commit();
					committed = true;
				}
				fMixer->Scene().GetPosition(i, track.x, track.y);
				track.path.clear();
			}
			fEngine->Unlock();
			SelectionChanged();
			break;
		}
		case kMsgClick:
			fSettings.click = !fSettings.click;
			break;
		case kMsgCountIn:
			fSettings.countIn = !fSettings.countIn;
			break;
		case kMsgLatency:
			(new LatencyWindow(this, fEngine->OutputDelay(), fSettings.inputLatency,
				BMessenger(this)))->Show();
			break;
		case kMsgSetLatency:
		{
			int32 ms;
			if (message->FindInt32("ms", &ms) == B_OK)
				fSettings.inputLatency = std::max(int32(0), std::min(int32(2000), ms));
			break;
		}
		case kMsgToggleArm:
		{
			// a track's R, or the menu: the (first) selected track
			int32 track = -1;
			if (message->FindInt32("track", &track) != B_OK) {
				for (int i = 0; i < fMixer->Scene().CountTracks() && track < 0; i++) {
					if (fMixer->Scene().IsSelected(i))
						track = i;
				}
			}
			if (track >= 0)
				SetArmed(track == fRecord.armed ? -1 : track);
			break;
		}
		case kMsgZoomStage:
		{
			int32 direction;
			if (message->FindInt32("dir", &direction) == B_OK)
				fMixer->Zoom(direction);
			break;
		}

		case kMsgMuteSelection:
			fMixer->MuteSelection();
			break;
		case kMsgSoloSelection:
			fMixer->SoloSelection();
			break;
		case kMsgSelectAllTracks:
		{
			fMixer->Scene().SelectAll();
			BMessage selection(kMsgMixerSelection);
			for (int i = 0; i < fMixer->Scene().CountTracks(); i++)
				selection.AddInt32("track", i);
			fTrackArea->SetSelection(selection);
			break;
		}

		case kMsgFastDisplay:
			fTrackArea->SetFastDisplay(!fTrackArea->FastDisplay());
			break;
		case kMsgUndo:
		case kMsgRedo:
			if (fEngine->Lock()) {
				if (message->what == kMsgUndo)
					fEditor->Undo();
				else
					fEditor->Redo();
				fEngine->Unlock();
			}
			ProjectStructureChanged(true);
			break;
		case kMsgClear:
		case kMsgCut:
		case kMsgCopy:
		case kMsgPaste:
		{
			pmx::ClipSelection clips;
			size_t pasteTrack, pasteAfter;
			fTrackArea->GetEditTargets(clips, pasteTrack, pasteAfter);
			if (fEngine->Lock()) {
				if (message->what == kMsgCopy)
					fEditor->Copy(clips);
				else if (message->what == kMsgCut)
					fEditor->Cut(clips);
				else if (message->what == kMsgClear)
					fEditor->Clear(clips);
				else
					fEditor->Paste(pasteTrack, pasteAfter);
				fEngine->Unlock();
			}
			if (message->what != kMsgCopy)
				fTrackArea->ProjectChanged(message->what == kMsgPaste);
			break;
		}
		case kMsgNewTrack:
		{
			// do_add_channel(char*, TrackViewer*): where the pointer is on
			// the floor, if it's over the 3D view
			float x = 0, y = 100;
			if (StageVisible())
				fMixer->FloorUnderPointer(x, y);
			if (fEngine->Lock()) {
				size_t index = fEditor->NewTrack(B_TRANSLATE("untitled"));
				pmx::Track& track = fEngine->Project().tracks[index];
				track.x = x;
				track.y = y;
				track.addedByHand = true;
				fEngine->Unlock();
			}
			ProjectStructureChanged();
			break;
		}
		case kMsgDeleteTracks:
		{
			std::set<size_t> tracks;
			for (int t : fTrackArea->SelectedTracks())
				tracks.insert(size_t(t));
			for (int i = 0; i < fMixer->Scene().CountTracks(); i++) {
				if (fMixer->Scene().IsSelected(i))
					tracks.insert(size_t(i));
			}
			if (tracks.empty())
				break;
			if (fEngine->Lock()) {
				fEditor->DeleteTracks(tracks);
				fEngine->Unlock();
			}
			ProjectStructureChanged();
			break;
		}
		case kMsgTicksFromSelection:
		case kMsgTicksFromSample:
		{
			float length = 0;
			if (fEngine->Lock()) {
				const pmx::Project& project = fEngine->Project();
				if (message->what == kMsgTicksFromSelection)
					length = project.selectionEnd - project.selectionStart;
				else {
					// TrackView::DoBPM1: the (last) selected clip's length
					pmx::ClipSelection clips;
					size_t a, b;
					fTrackArea->GetEditTargets(clips, a, b);
					for (const pmx::ClipRef& ref : clips) {
						if (ref.first < project.tracks.size()
							&& ref.second < project.tracks[ref.first].clips.size())
							length = pmx::ClipLength(project.tracks[ref.first].clips[ref.second]);
					}
				}
				fEngine->Unlock();
			}
			if (length > 0)
				(new BeatWindow(this, length, BMessenger(this)))->Show();
			break;
		}
		case kMsgSetTicks:
		{
			float ticks;
			if (message->FindFloat("ticks", &ticks) == B_OK) {
				fTrackArea->SetTicks(ticks);
				SyncModernControls();
			}
			break;
		}
		case kMsgShowHideBeatMarks:
			fTrackArea->SetBeatMarks(!fTrackArea->BeatMarks());
			break;
		case kMsgEditClip:
		{
			// SimpleObject::Edit: one editor per clip, brought forward if open
			int32 track, clip;
			if (message->FindInt32("track", &track) != B_OK
				|| message->FindInt32("clip", &clip) != B_OK)
				break;
			bool found = false;
			for (BMessenger& editor : fEditors) {
				BLooper* looper = NULL;
				editor.Target(&looper);
				EditWindow* window = dynamic_cast<EditWindow*>(looper);
				if (editor.IsValid() && window != NULL && window->Lock()) {
					if (window->Edits(size_t(track), size_t(clip))) {
						window->Activate();
						found = true;
					}
					window->Unlock();
				}
			}
			if (!found) {
				EditWindow* window = new EditWindow(fEngine, size_t(track), size_t(clip),
					BMessenger(this));
				fEditors.push_back(BMessenger(window));
				window->Show();
			}
			break;
		}
		case kMsgDropRefs:
			DropRefs(message);
			break;
		case kMsgRunScript:
		{
			const char* script;
			if (message->FindString("script", &script) == B_OK)
				RunScript(script);
			break;
		}
		case kMsgClipsChanged:
			fTrackArea->RedrawLanes();
			break;
		case kMsgPause:
			fEngine->SetPaused(!fEngine->IsPaused());
			SyncModernControls();
			break;

		case kMsgMasterChanged:
		{
			float value;
			if (message->FindFloat("value", &value) == B_OK && fEngine->Lock()) {
				fEngine->Mixer().SetMasterGain(pmx::MasterGainForSlider(value));
				fEngine->Unlock();
				// both looks' sliders follow
				fMasterValue = value;
				fStatus->Slider()->SetValue(value);
				if (fToolbar != NULL)
					fToolbar->SetMaster(value);
			}
			break;
		}
		case kMsgMixerSelection:
			fTrackArea->SetSelection(*message);
			SelectionChanged();
			break;
		case kMsgMuteSoloChanged:
			fTrackArea->RedrawLanes();
			SelectionChanged();
			break;
		case kMsgTrackSelection:
		{
			pmx::Scene& scene = fMixer->Scene();
			scene.DeselectAll();
			int32 track;
			for (int32 i = 0; message->FindInt32("track", i, &track) == B_OK; i++)
				scene.Select(track, true);
			SelectionChanged();
			break;
		}

		default:
			BWindow::MessageReceived(message);
	}
}

bool
MainWindow::QuitRequested()
{
	if (fRecord.recording)
		StopRecording();
	if (fModern)
		fSettings.modernFrame = Frame();
	else
		fSettings.classicFrame = Frame();
	fSettings.modern = fModern;
	fSettings.Save();
	be_app->PostMessage(B_QUIT_REQUESTED);
	return true;
}
