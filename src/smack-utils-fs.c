#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "smack-utils-fs.h"


static char *internal_smack_mount_point = NULL;
static int internal_smack_mount_point_computed = 0;

/*
 * Returns the mount point of the the smack filesystem
 * or NULL in case of error.
 * To get the default mount point, pass "/proc/mounts"
 * as mountfile argument.
 */
static char *compute_smack_mount_point(const char *mountfile)
{
	int file, state, pos;
	ssize_t readen;
	char buffer[2048], *iter, *end, mount[1024];

	/* opens the mount file */
	file = open(mountfile, O_RDONLY);
	if (file < 0)
		return NULL;

	/* parse the file, searching /^smackfs +([^ ]+) / */
	pos = 0;
	state = 0;
	for (;;) {
		readen = read(file, buffer, sizeof buffer);
		if (readen <= 0) {
			close(file);
			return NULL;
		}
		for (iter = buffer, end = buffer + readen ; iter != end ; iter++) {
			switch(state) {
			case 0: if (*iter == 's') { state = 2; break; }
			case 1: if (*iter != '\n') state = 0; break;
			case 2: state = (*iter == 'm') ? 3 : 1; break;
			case 3: state = (*iter == 'a') ? 4 : 1; break;
			case 4: state = (*iter == 'c') ? 5 : 1; break;
			case 5: state = (*iter == 'k') ? 6 : 1; break;
			case 6: state = (*iter == 'f') ? 7 : 1; break;
			case 7: state = (*iter == 's') ? 8 : 1; break;
			case 8: state = (*iter == ' ') ? 9 : 1; break;
			case 9: if (*iter == ' ') break; else state = 10;
			case 10:
				if (pos >= sizeof mount) {
					close(file);
					return NULL;
				}
				if (*iter == ' ' || *iter == '\n') {
					mount[pos] = 0;
					close(file);
					return strdup(mount);
				}
				mount[pos++] = *iter; 
				break;
			}
		}
	}
}

const char *smack_fs_mount_point()
{
	if (!internal_smack_mount_point_computed) {
		internal_smack_mount_point = compute_smack_mount_point("/proc/mounts");
		internal_smack_mount_point_computed = 1;
	}
	return internal_smack_mount_point;
}



