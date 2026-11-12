// setout: make the null audio device ("None Out") BeOS 5's default audio
// output, which the Media preferences won't offer. Built in the guest.
#include <MediaRoster.h>
#include <MediaAddOn.h>
#include <Application.h>
#include <stdio.h>
#include <string.h>
int main()
{
	BApplication app("application/x-vnd.powermix-setout");
	BMediaRoster *r = BMediaRoster::Roster();
	dormant_node_info d[32]; int32 n = 32;
	media_format f;
	r->GetDormantNodes(d, &n, NULL, NULL, NULL, B_PHYSICAL_OUTPUT, 0);
	for (int i = 0; i < n; i++) {
		printf("dormant %d: %s\n", i, d[i].name);
		if (strcmp(d[i].name, "None Out") == 0)
			printf("SetAudioOutput: %s\n", strerror(r->SetAudioOutput(d[i])));
	}
	return 0;
}
