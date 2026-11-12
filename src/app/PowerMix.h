// PowerMix.h: application-wide constants. Menu commands keep 3dmiX's
// message codes (TWaveWindow's constructor).
#ifndef POWERMIX_H
#define POWERMIX_H

#include <SupportDefs.h>

#define APP_SIGNATURE "application/x-vnd.idumych-PowerMix"
// 3dmiX's project type, so the R5 mixes open with PowerMix
#define PROJECT_TYPE "application/x-vnd.Be.3DSoundMixer_Master"

enum {
	kMsgSaveProjectAs = 0x100,
	kMsgSaveProject = 0x101,
	kMsgSaveMixAsWave = 0x102,
	kMsgOpenProject = 0x103,
	kMsgAbout = 0x104,
	kMsgQuit = 0x105,

	kMsgUndo = 0x201,
	kMsgClear = 0x202,
	kMsgCut = 0x203,
	kMsgCopy = 0x204,
	kMsgPaste = 0x205,
	kMsgRedo = 0x206,		// PowerMix's: 3dmiX had one level of undo

	kMsgShowHideMixer = 0x300,
	kMsgShowHideControls = 0x301,
	kMsgShowHideBeatMarks = 0x302,
	kMsgFastDisplay = 0x303,

	kMsgMuteSelection = 0x400,
	kMsgSoloSelection = 0x401,
	kMsgSelectAllTracks = 0x402,
	kMsgNewTrack = 0x403,
	kMsgTicksFromSelection = 0x501,
	kMsgTicksFromSample = 0x502,
	kMsgDeleteTracks = 0x503,
	kMsgPause = 0x600,

	// PowerMix's own
	kMsgAnimate = 'Anim',
	kMsgMixerSelection = 'MSel',
	kMsgTrackSelection = 'TSel',
	kMsgMuteSoloChanged = 'MuSo',
	kMsgMasterChanged = 'Mstr',
	kMsgProjectChanged = 'PChg',
	kMsgOpenPanelRefs = 'ORef',
	kMsgSavePanel = 'SvPn',
	kMsgSaveWavePanel = 'SvWv',
	kMsgEditClip = 'EdCl',
	kMsgClipsChanged = 'ClCh',
	kMsgSetTicks = 'STik',
	kMsgDropRefs = 'Drop',
	kMsgRunScript = 'Grun',

	// the modern look
	kMsgModernLook = 'Modn',
	kMsgViewMode = 'VMod',		// "mode": 0 stage, 1 split, 2 timeline
	kMsgSeekStart = 'Strt',
	kMsgSelectTrack = 'SlTk',	// "track", "extend"
	kMsgToggleMute = 'TgMu',	// "track"
	kMsgToggleSolo = 'TgSo',	// "track"
	kMsgSetBeats = 'SBet',		// "beats"
	kMsgZoomStage = 'ZmSt',		// "dir": 1 in, -1 out, 0 reset the angle

	// recording
	kMsgRecord = 'Recd',		// Tracks > Record: start or stop a take
	kMsgToggleArm = 'TgAr',		// "track"; without one, the selected track
	kMsgLatency = 'Ltcy',		// Misc > Recording latency...
	kMsgSetLatency = 'SLtc',	// "ms": the extra input delay
	kMsgClick = 'Clik',			// Tracks > Click while recording
	kMsgCountIn = 'Cnti',		// Tracks > Count-in before recording
};

#endif
