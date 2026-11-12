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
	editor.Undo();					// undo twice = redo
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

	printf(sFailures ? "%d FAILED\n" : "all passed\n", sFailures);
	return sFailures ? 1 : 0;
}
