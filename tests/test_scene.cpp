// Host tests for src/core/Scene and Palette.
#include "Palette.h"
#include "Scene.h"

#include <cmath>
#include <cstdio>

using namespace pmx;

static int sFailures = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); sFailures++; } } while (0)

static float Width(const std::string& s) { return float(s.size() * 6); }

static void TestTables()
{
	CHECK(Gamma(0) == 0 && Gamma(1) == 5 && Gamma(128) == 158 && Gamma(255) == 255);
	CHECK(ColorMix(229, 0) == 173);	// trunc(trunc(217.55) * 0.8)
	const ColorTables& t = ColorTables::Get();
	// a red face over black is red, over red brighter red
	Rgb c = kSystemPalette[t.MapperAt(kHueRed * 4 + 2).index[0]];
	CHECK(c.r > 100 && c.g == 0 && c.b == 0);
	CHECK(kSystemPalette[t.RampAt(kRampBlue).index[31]].b > 200);
	CHECK(t.RampAt(kRampBlue).index[0] == 0);
}

static void TestScene()
{
	Scene scene;
	scene.AddTrack("a", 0, 0);
	scene.AddTrack("b", -200, 100);
	CHECK(scene.CountTracks() == 2);
	float x, y;
	scene.GetPosition(1, x, y);
	CHECK(x == -200 && y == 100);

	scene.Render(Width);
	// the floor grid is drawn, the area above the horizon is black
	int lit = 0;
	for (int i = 300 * Scene::kWidth; i < 400 * Scene::kWidth; i++)
		lit += scene.Frame()[i] != 0;
	CHECK(lit > 5000);
	CHECK(scene.Frame()[10 * Scene::kWidth + 512] == 0);
	CHECK(scene.Labels().size() == 2);
	CHECK(scene.Labels()[1].text == "1-b");

	// the column of track 0 stands in the middle of the frame
	CHECK(scene.HitTest(512, 230) == 0);
	CHECK(scene.HitTest(5, 5) == -1);

	// dragging moves only the selection; walls at +-250
	scene.SetCamera(0.32f, 0);
	scene.Select(0, true);
	CHECK(scene.HasSelection() && scene.IsSelected(0) && !scene.IsSelected(1));
	scene.DragSelection(10, 0, false);
	scene.GetPosition(0, x, y);
	CHECK(x == 10);
	// a wall pulls back 90% of the overshoot (and 1) per move
	scene.DragSelection(400, 0, false);
	scene.GetPosition(0, x, y);
	CHECK(std::fabs(x - (410 + (250 - 410) * 0.9f - 1)) < 0.01f);
	scene.GetPosition(1, x, y);
	CHECK(x == -200);
	scene.MoveSelection(0, 15);
	scene.DeselectAll();
	CHECK(!scene.HasSelection());

	// removing a track renumbers the ones after it
	scene.RemoveTrack(0);
	CHECK(scene.CountTracks() == 1);
	scene.GetPosition(0, x, y);
	CHECK(x == -200 && y == 100);
	scene.Render(Width);
	CHECK(scene.Labels().size() == 1 && scene.Labels()[0].text == "0-b");

	// camera: a drag rotates, a flick spins down to rest
	float yaw = scene.Yaw();
	scene.Rotate(50, 0);
	CHECK(std::fabs(scene.Yaw() - (yaw - 0.1f)) < 1e-5f);
	scene.SetSpin(0, 0.05f);
	for (int i = 0; i < 400; i++)
		scene.Step();
	float rest = scene.Yaw();
	scene.Step();
	CHECK(scene.Yaw() == rest);
}

int main()
{
	TestTables();
	TestScene();
	printf(sFailures ? "%d FAILED\n" : "all passed\n", sFailures);
	return sFailures ? 1 : 0;
}
