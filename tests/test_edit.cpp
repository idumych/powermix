// Host tests for src/core/Edit.
#include "Edit.h"

#include <cmath>
#include <cstdio>

using namespace pmx;

static int sFailures = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); sFailures++; } } while (0)
#define NEAR(a, b) (std::fabs((a) - (b)) < 1e-4f)

static Clip MakeClip(float start, float end, float loop)
{
	Clip c;
	c.source = "s";
	c.start = start;
	c.end = end;
	c.loopIn = 0;
	c.loopOut = loop;
	c.samples.assign(size_t(loop * kSampleRate) + 10, 1);
	return c;
}

int main()
{
	// grid snapping
	CHECK(NEAR(ToGrid(1.26f, 0.5f), 1.5f));
	CHECK(NEAR(ToGrid(1.24f, 0.5f), 1.0f));
	CHECK(NEAR(ToGrid(1.24f, 0), 1.24f));

	Clip c = MakeClip(1, 5, 2);
	SetClipPos(c, 3, 0);
	CHECK(NEAR(c.start, 3) && NEAR(c.end, 7));
	SetClipLength(c, 1, 0);			// shorter than the loop: the loop shrinks
	CHECK(NEAR(c.end, 4) && NEAR(ClipLoop(c), 1));
	SetClipSkip(c, 0.5f);
	CHECK(NEAR(c.loopIn, 0.5f) && NEAR(c.loopOut, 1));
	SetClipSkip(c, 5);				// clamped below the loop's end
	CHECK(NEAR(c.loopIn, 0.99f));
	SetClipLoop(c, 10, 0);			// clamped to the clip's length
	CHECK(NEAR(ClipLoop(c), ClipLength(c)));

	// clips never overlap, later ones are pushed back
	Project p;
	Track t;
	t.name = "a";
	t.clips.push_back(MakeClip(0, 2, 1));
	t.clips.push_back(MakeClip(1, 3, 1));
	t.clips.push_back(MakeClip(10, 11, 1));
	RedoTime(t);
	CHECK(NEAR(t.clips[1].start, 2) && NEAR(t.clips[1].end, 4));
	CHECK(NEAR(t.clips[2].start, 10));
	p.tracks.push_back(t);

	Editor editor(&p);
	ClipSelection selection = {{0, 0}};
	editor.Copy(selection);
	editor.Paste(0, 1);				// after the first clip
	CHECK(p.tracks[0].clips.size() == 4);
	CHECK(NEAR(p.tracks[0].clips[1].start, 2) && NEAR(p.tracks[0].clips[2].start, 4));
	editor.Undo();
	CHECK(p.tracks[0].clips.size() == 3);
	editor.Undo();					// nothing more to undo (3dmiX's redid)
	CHECK(p.tracks[0].clips.size() == 3 && !editor.CanUndo());
	editor.Redo();
	CHECK(p.tracks[0].clips.size() == 4);

	selection = {{0, 0}, {0, 3}};
	editor.Cut(selection);
	CHECK(p.tracks[0].clips.size() == 2 && selection.empty());
	CHECK(editor.HasClipboard());

	size_t n = editor.NewTrack("a");
	CHECK(n == 1 && p.tracks[1].name == "a 2");
	editor.DeleteTracks({0});
	CHECK(p.tracks.size() == 1 && p.tracks[0].name == "a 2");
	editor.Undo();
	CHECK(p.tracks.size() == 2);

	// import: stereo 22050 Hz -> mono 44100 Hz
	float stereo[8] = {0.5f, 0.5f, 1, 0, -1, -1, 0, 0};
	std::vector<int16_t> s = ToClipSamples(stereo, 4, 2, 22050);
	CHECK(s.size() == 8 && s[0] == 16383 && s[2] == 16383 && s[4] == -32767);
	Clip made = MakeClip("x.wav", s);
	CHECK(made.source == "x.wav" && NEAR(made.loopOut, 0.01f));	// at least 0.01 s

	// a recording: clips 0-2 and 4-6, a 3 s take from 1 s
	Track r;
	r.clips.push_back(MakeClip(0, 2, 2));
	r.clips.push_back(MakeClip(4, 6, 2));
	Clip take = MakeClip(0, 3, 3);
	size_t at = PlaceRecording(r, 1, take);
	CHECK(at == 1 && r.clips.size() == 3);
	CHECK(NEAR(r.clips[0].end, 1));					// cut where the take began
	CHECK(NEAR(r.clips[1].start, 1) && NEAR(r.clips[1].end, 4));
	CHECK(NEAR(r.clips[2].start, 4) && NEAR(r.clips[2].end, 6));	// not reached: stays
	// a take reaching a later clip pushes it back
	at = PlaceRecording(r, 3.5f, MakeClip(0, 1, 1));
	CHECK(at == 2 && NEAR(r.clips[1].end, 3.5f) && NEAR(r.clips[2].start, 3.5f));
	CHECK(NEAR(r.clips[3].start, 4.5f) && NEAR(r.clips[3].end, 6.5f));
	// into an empty track, and before every clip
	Track empty;
	CHECK(PlaceRecording(empty, 2, MakeClip(0, 1, 1)) == 0 && NEAR(empty.clips[0].start, 2));
	CHECK(PlaceRecording(empty, 0, MakeClip(0, 0.5f, 0.5f)) == 0 && NEAR(empty.clips[1].start, 2));

	// undo history: several steps back, and forward again
	{
		Project h;
		Editor history(&h);
		CHECK(!history.CanUndo() && !history.CanRedo());
		for (int i = 0; i < 5; i++)
			history.NewTrack("t");
		CHECK(h.tracks.size() == 5 && history.UndoSteps() == 5);
		history.Undo();
		history.Undo();
		history.Undo();
		CHECK(h.tracks.size() == 2 && history.CanRedo());
		history.Redo();
		CHECK(h.tracks.size() == 3);
		history.NewTrack("u");			// a new change ends the redo
		CHECK(h.tracks.size() == 4 && !history.CanRedo());
		while (history.CanUndo())
			history.Undo();
		CHECK(h.tracks.empty());
		history.Reset();
		CHECK(!history.CanRedo());
		// a long history of big sounds keeps under the limit
		Track big;
		big.clips.push_back(MakeClip(0, 1, 1));
		big.clips[0].samples.assign(20u << 20, 0);	// 40 MB
		h.tracks.push_back(big);
		for (int i = 0; i < 12; i++)
			history.Commit();
		CHECK(history.UndoSteps() >= 5 && history.UndoSteps() <= 7);
	}

	// floor automation: writing replaces the stretch moved through
	{
		Track m;
		m.x = 10;
		float px, py;
		m.PositionAt(5, px, py);
		CHECK(px == 10);						// no path: where it stands
		for (int i = 0; i <= 10; i++)
			WritePathPoint(m, i - 1.0f, float(i), float(i * 10), 0);
		CHECK(m.path.size() == 11);
		m.PositionAt(2.5, px, py);
		CHECK(NEAR(px, 25));					// between points, a straight line
		m.PositionAt(-1, px, py);
		CHECK(px == 0);							// rests before the first
		// a second pass from 3 s to 6 s replaces the points in (3, 6]
		WritePathPoint(m, 3, 4.5f, -100, 0);
		WritePathPoint(m, 4.5f, 6, -100, 0);
		CHECK(m.path.size() == 11 - 3 + 2);
		m.PositionAt(5, px, py);
		CHECK(NEAR(px, -100));
		m.PositionAt(7, px, py);
		CHECK(NEAR(px, 70));					// later points stay
		ShiftPath(m, 200, 0);
		CHECK(m.path.back().x == 256);			// clamped to the floor
	}

	// latency compensation: earlier by the shift; nothing before 0
	std::vector<int16_t> takeSamples(44100, 7);
	float takeStart = 1.0f;
	AlignTake(takeStart, takeSamples, 0.05f);
	CHECK(NEAR(takeStart, 0.95f) && takeSamples.size() == 44100);
	takeStart = 0.02f;
	AlignTake(takeStart, takeSamples, 0.05f);
	CHECK(NEAR(takeStart, 0) && takeSamples.size() == 44100 - 1323);	// 0.03 s dropped

	printf(sFailures ? "%d FAILED\n" : "all passed\n", sFailures);
	return sFailures ? 1 : 0;
}
