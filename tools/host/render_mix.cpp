// render_mix PROJECT OUT.wav [MASTER]: mixes a project's time selection
// once, as File ▸ Save mix as WAVE does, for comparing with the original.
#include "Mixer.h"
#include "Project.h"
#include "WaveFile.h"

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
	Mixer mixer(&project);
	mixer.SetMasterGain(MasterGainForSlider(argc > 3 ? float(atof(argv[3])) : 50));
	mixer.SetRange(project.selectionStart, project.selectionEnd);
	mixer.Seek(project.selectionStart);
	mixer.SetLoop(false);
	mixer.SettlePan();
	std::vector<int16_t> frames;
	int16_t block[2 * kBlockFrames];
	while (mixer.NextBlock(block))
		frames.insert(frames.end(), block, block + 2 * kBlockFrames);
	return WriteWave(argv[2], frames) == 0 ? 0 : 1;
}
