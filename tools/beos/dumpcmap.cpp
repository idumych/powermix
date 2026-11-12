// dumpcmap: writes BeOS's screen colour map (palette and 15-bit inverse
// map) to /tmp/cmap.bin, to study how IndexForColor picks colours.
#include <Application.h>
#include <Screen.h>
#include <stdio.h>
int main()
{
	BApplication app("application/x-vnd.powermix-dumpcmap");
	BScreen screen;
	const color_map* map = screen.ColorMap();
	FILE* f = fopen("/tmp/cmap.bin", "wb");
	fwrite(map->color_list, sizeof(rgb_color), 256, f);
	fwrite(map->index_map, 1, 32768, f);
	fclose(f);
	printf("ok\n");
	return 0;
}
