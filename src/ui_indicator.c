#include "ui_indicator.h"

void indicator_start() {
	const char *anim = "|/-\\";
	uint32_t anim_len = strlen(anim);
	uint32_t frame = 0;

	while(1) {
		struct site_pair *pair = site_get_current_pair();
		char il = ' ';
		char ir = ' ';

		if((pair->left != NULL) && pair->left->busy) {
			il = anim[frame % anim_len];
		}

		if((pair->right != NULL) && pair->right->busy) {
			ir = anim[frame % anim_len];
		}

		char *s_prefix = generate_ui_prompt(il, ir);

		rl_set_prompt(s_prefix);
		rl_redisplay();

		free(s_prefix);

		frame++;
		usleep(250000);
	}
}
