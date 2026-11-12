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

#include <cstdio>

#include "AudioEngine.h"
#include "MixerView.h"
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
	fControlsShown(false)
{
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

	fMixer = new MixerView(BRect(0, 0, 1023, kMixerHeight), engine);
	fInner->AddChild(fMixer);

	fTrackArea = new TrackArea(BRect(0, kMixerHeight, width, height + 50 + kMixerHeight),
		engine);
	fInner->AddChild(fTrackArea);
	fMixer->SetSelectionTarget(this);

	fStatus = new StatusView(BRect(0, 0, 63, 32000), this);
	fOuter->AddChild(fStatus);
	fOuter->ScrollBy(66, 0);

	BMessage animate(kMsgAnimate);
	fRunner = new BMessageRunner(BMessenger(this), &animate, 35000);
}

MainWindow::~MainWindow()
{
	delete fRunner;
	delete fOpenPanel;
	delete fSavePanel;
	delete fWavePanel;
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
	menu->AddItem(new BMenuItem(B_TRANSLATE("Undo"), new BMessage(kMsgUndo), 'Z'));
	menu->AddSeparatorItem();
	menu->AddItem(new BMenuItem(B_TRANSLATE("Clear"), new BMessage(kMsgClear)));
	menu->AddItem(new BMenuItem(B_TRANSLATE("Cut"), new BMessage(kMsgCut), 'X'));
	menu->AddItem(new BMenuItem(B_TRANSLATE("Copy"), new BMessage(kMsgCopy), 'C'));
	menu->AddItem(new BMenuItem(B_TRANSLATE("Paste"), new BMessage(kMsgPaste), 'V'));
	bar->AddItem(menu);

	menu = new BMenu(B_TRANSLATE("Display"));
	menu->AddItem(new BMenuItem(B_TRANSLATE("Show/hide mixer"),
		new BMessage(kMsgShowHideMixer), 'T'));
	menu->AddItem(new BMenuItem(B_TRANSLATE("Show/hide controls"),
		new BMessage(kMsgShowHideControls), 'U'));
	menu->AddItem(new BMenuItem(B_TRANSLATE("Show/hide beat marks"),
		new BMessage(kMsgShowHideBeatMarks), 'B'));
	menu->AddSeparatorItem();
	menu->AddItem(new BMenuItem(B_TRANSLATE("Fast display"), new BMessage(kMsgFastDisplay)));
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
	bar->AddItem(menu);

	menu = new BMenu(B_TRANSLATE("Misc"));
	menu->AddItem(new BMenuItem(B_TRANSLATE("Ticks from time selection"),
		new BMessage(kMsgTicksFromSelection)));
	menu->AddItem(new BMenuItem(B_TRANSLATE("Ticks from sample length"),
		new BMessage(kMsgTicksFromSample)));
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
	do {
		bigtime_t start = system_time();
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
}

void
MainWindow::ShowControls()
{
	if (Lock()) {
		if (!fControlsShown)
			SwitchControls();
		Unlock();
	}
}

void
MainWindow::Animate()
{
	fMixer->Animate(fMixerShown);
	fStatus->SetLevel(fEngine->LeftLevel(), fEngine->RightLevel());
	double time = 0;
	if (fEngine->Lock()) {
		time = fEngine->Mixer().Time();
		fEngine->Unlock();
	}
	fTrackArea->SetTime(time);
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
	mixer.SetMasterGain(pmx::MasterGainForSlider(fStatus->Slider()->Value()));
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
			SwitchMixer();
			break;
		case kMsgShowHideControls:
			SwitchControls();
			break;

		case kMsgMuteSelection:
			fMixer->MuteSelection();
			break;
		case kMsgSoloSelection:
			fMixer->SoloSelection();
			break;
		case kMsgSelectAllTracks:
			fMixer->Scene().SelectAll();
			break;

		case kMsgPause:
			fEngine->SetPaused(!fEngine->IsPaused());
			break;

		case kMsgMasterChanged:
		{
			float value;
			if (message->FindFloat("value", &value) == B_OK && fEngine->Lock()) {
				fEngine->Mixer().SetMasterGain(pmx::MasterGainForSlider(value));
				fEngine->Unlock();
			}
			break;
		}
		case kMsgMixerSelection:
			fTrackArea->SetSelection(*message);
			break;
		case kMsgMuteSoloChanged:
			fTrackArea->Invalidate();
			break;

		default:
			BWindow::MessageReceived(message);
	}
}

bool
MainWindow::QuitRequested()
{
	be_app->PostMessage(B_QUIT_REQUESTED);
	return true;
}
