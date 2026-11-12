// Scene.h: 3dmiX's 3D mixer view (TSoundView's drawing half): the floor
// grid, one translucent column per track, labels, the camera, hit testing
// and dragging. Renders palette indices into a 1024x480 frame exactly as
// the original's software renderer does; the UI converts it to RGB and
// draws the label text. Portable: no Haiku headers.
#ifndef POWERMIX_SCENE_H
#define POWERMIX_SCENE_H

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace pmx {

class Scene {
public:
	static constexpr int kWidth = 1024;
	static constexpr int kHeight = 480;

	Scene();

	// The frame: kWidth x kHeight palette indices, row by row.
	const uint8_t* Frame() const { return fFrame.data(); }
	// TSoundView::Draw: the frame's x offset in a window this wide.
	static int FrameOffset(float windowWidth);

	// Tracks (do_add_channel, delete_channel). Index = track number.
	void AddTrack(const std::string& name, float x, float y);
	// do_add_channel(char*, TrackViewer*): a track added by hand; its level
	// bar is drawn in blue instead of green
	void AddNewTrack(const std::string& name, float x, float y);
	// invert_map: the floor point (x, depth) under a frame point
	bool FloorPoint(float frameX, float frameY, float& x, float& y) const;
	void RemoveTrack(int index);
	void RemoveAllTracks();
	int CountTracks() const { return int(fTracks.size()); }
	void SetName(int index, const std::string& name);
	// The track's place on the floor (x, and y = depth), as the mix uses it
	void GetPosition(int index, float& x, float& y) const;
	void SetPosition(int index, float x, float y);

	// SetChannelLevel: the level bar inside the column, 2..15 (Mixer's
	// TrackLevel); a silent track's bar drops to the floor.
	void SetLevel(int index, float level);
	// Muted, or silenced by another track's solo (SetMuteSolo): the level
	// bar drops, and the column shrinks by x0.68 three times per state,
	// one step per Step(). Growing back multiplies by 1.4705882, so the
	// height drifts a little, as in the original.
	void SetMuted(int index, bool muted);
	void SetSoloSilenced(int index, bool silenced);
	bool IsMuted(int index) const;

	bool IsSelected(int index) const;
	void Select(int index, bool selected);
	void SelectAll();
	void DeselectAll();
	bool HasSelection() const;

	// Camera. Drag on empty floor: Rotate(dx, dy) per mouse move.
	void Rotate(float dx, float dy);
	void ZoomIn();			// "+": focal length x 1.05, up to 900
	void ZoomOut();			// "-": / 1.05, down to 30
	void MoveCameraBack();	// ","
	void MoveCameraNearer();	// "."
	void SetSpin(float pitch, float yaw);	// inertia after a drag
	void SetCamera(float pitch, float yaw) { fPitch = pitch; fYaw = yaw; }
	void ToggleDemo() { fDemo = !fDemo; }	// "D": slow automatic rotation
	void ToggleParticles() { fParticles = !fParticles; }	// "P"

	// One animation frame (TSoundView::Animate): demo rotation and spin.
	void Step();

	// find_hit: the track under a frame point, or -1. Like the original it
	// returns the farthest face hit, not the nearest.
	int HitTest(float x, float y);
	// handle_hit's drag: moves the selection by a mouse delta, rotated by
	// the camera's yaw; depthOnly = Shift held. Keeps tracks inside +-250.
	void DragSelection(float dx, float dy, bool depthOnly);
	// The arrow keys: 15 units on the floor.
	void MoveSelection(float dx, float dz);

	// Renders a frame. textWidth measures a label in the UI's font.
	struct Label {
		int x, y;			// pen position of the text's baseline start
		std::string text;	// "3-bass"
		bool selected;
	};
	void Render(const std::function<float(const std::string&)>& textWidth);
	const std::vector<Label>& Labels() const { return fLabels; }

	// For a vector renderer (PowerMix's modern look): the floor's lines,
	// the columns' faces (far to near, as Render paints them) and the
	// labels' anchors, projected into frame coordinates like Render's
	struct FrameLine {
		float x1, y1, x2, y2;
		float depth;		// larger is nearer
		int kind;			// 0 the floor's grid, 1 the marks in front of it
	};
	struct FrameFace {
		float x[4], y[4];
		float depth;
		int track;
		int part;			// 0 top, 1 side, 2 bottom, 3 the level bar
	};
	struct FrameLabel {
		float anchorX, anchorY;		// the column's top
		float x, y;					// above it
		int track;
	};
	void ProjectScene(std::vector<FrameLine>& lines, std::vector<FrameFace>& faces,
		std::vector<FrameLabel>& labels);
	void ProjectPoint(float x, float y, float z, float& fx, float& fy, float& depth) const;

	// Camera angles, for tests
	float Pitch() const { return fPitch; }
	float Yaw() const { return fYaw; }
	float Focal() const { return fFocal; }

private:
	struct Vector {
		float x1, y1, z1, x2, y2, z2;
		int color;
		float sx1, sy1, sx2, sy2;
	};
	struct Vertex {
		float x, y, z;
		float sx, sy, depth;
		int owner;			// track, or track + 0x400 for the anchor
		uint32_t tag;		// 'itop', 'ttop', 'bomm' ...
	};
	struct Poly {
		int v[4];
		int base;		// mapper as created; selection adds one level
		int mapper;
		int owner;
	};
	struct Track {
		std::string name;
		bool selected = false;
		bool muted = false;
		bool soloSilenced = false;
		float level = 2;
		int squash = 0;		// x0.68 steps applied to the column
	};
	struct Particle {
		float x, y, z;
		int sx, sy;
		uint8_t color;
	};

	void BuildVectors();
	void AddTrackWithBar(const std::string& name, float x, float y, int barMapper);
	void Project(float x, float y, float z, float& sx, float& sy, float& depth) const;
	void MoveTrack(int index, float dx, float dy, float dz);
	void KeepInside();
	void UpdateMappers(int index);
	bool IsSilent(int index) const;
	void SquashStep(int index, float factor);

	void Clear();
	void Line(int x1, int y1, int x2, int y2, const uint8_t* ramp, bool antialias);
	void AntiLine(int x1, int y1, int x2, int y2, const uint8_t* ramp);
	void PlainLine(int x1, int y1, int x2, int y2, const uint8_t* ramp);
	bool Spans(const float* xs, const float* ys, int& top, int& bottom);
	void Fill4(const float* xs, const float* ys, const uint8_t* map);
	void DrawParticles();
	void SortPolys();

	std::vector<uint8_t> fFrame;
	std::vector<Vector> fVectors;
	std::vector<Vertex> fVertices;
	std::vector<Poly> fPolys;
	std::vector<Track> fTracks;
	std::vector<Label> fLabels;
	std::vector<Particle> fParticleList;
	std::vector<float> fParticleVelocity;	// x, y, z per particle
	std::vector<int> fSpanLeft, fSpanRight;

	float fRoll;		// field 0x1348, always 0
	float fPitch;		// 0x134c (the view uses -pitch)
	float fYaw;			// 0x1350
	float fFocal;		// 0x1358
	float fDistance;	// 0x38a78
	int fCenterY;		// 0x3ae98
	float fSpinPitch, fSpinYaw;
	bool fSpinning;
	bool fDemo;
	bool fParticles;
};

}	// namespace pmx

#endif
