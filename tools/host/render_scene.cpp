// render_scene PROJECT OUT.ppm [WINDOW_WIDTH] [LEVEL]: renders a project's
// 3D view as the mixer shows it (no label text), for comparing with
// screenshots of the original.
#include "Palette.h"
#include "Project.h"
#include "Scene.h"

#include <cstdio>
#include <cstdlib>

using namespace pmx;

int main(int argc, char** argv)
{
	if (argc < 3)
		return 2;
	Project project;
	std::string error;
	if (!ReadProject(argv[1], project, &error)) {
		fprintf(stderr, "%s\n", error.c_str());
		return 1;
	}
	// PMX_CMAP=vm/orig/cmap/cmap.bin: BeOS's own colour table, for checking
	if (const char* cmap = getenv("PMX_CMAP")) {
		FILE* f = fopen(cmap, "rb");
		uint8_t table[1024 + 32768];
		if (f && fread(table, 1, sizeof(table), f) == sizeof(table))
			SetInverseMap(table + 1024);
		if (f)
			fclose(f);
	}
	int width = argc > 3 ? atoi(argv[3]) : 700;
	float level = argc > 4 ? float(atof(argv[4])) : 2;
	Scene scene;
	if (argc > 6)
		scene.SetCamera(float(atof(argv[5])), float(atof(argv[6])));
	for (const Track& track : project.tracks)
		scene.AddTrack(track.name, track.x, track.y);
	for (int i = 0; i < scene.CountTracks(); i++)
		scene.SetLevel(i, level);
	scene.Render([](const std::string& s) { return float(s.size() * 6); });
	int offset = Scene::FrameOffset(float(width));
	FILE* f = fopen(argv[2], "wb");
	fprintf(f, "P6 %d %d 255\n", width, Scene::kHeight);
	for (int y = 0; y < Scene::kHeight; y++) {
		for (int x = 0; x < width; x++) {
			int fx = x - offset;
			uint8_t i = fx < Scene::kWidth ? scene.Frame()[y * Scene::kWidth + fx] : 0;
			const Rgb& c = kSystemPalette[i];
			fputc(c.r, f); fputc(c.g, f); fputc(c.b, f);
		}
	}
	fclose(f);
	for (const Scene::Label& l : scene.Labels())
		printf("label %d %d %s\n", l.x + offset, l.y, l.text.c_str());
	return 0;
}
