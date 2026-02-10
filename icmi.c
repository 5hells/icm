#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char *default_socket_path(void) {
    const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
    if (runtime_dir && runtime_dir[0] != '\0') {
        static char path[256];
        snprintf(path, sizeof(path), "%s/icm.sock", runtime_dir);
        return path;
    }
    return "/tmp/icm.sock";
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s sock [path]\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "sock") == 0) {
        const char *path = (argc >= 3) ? argv[2] : default_socket_path();
        printf("Starting icm with IPC socket: %s\n", path);
        if (access(path, F_OK) == 0) {
            fprintf(stderr, "Error: Socket file %s already exists. Is icm already running?\n", path);
            return 1;
        }
        if (access("/usr/bin/icm", X_OK) != 0 || access("/bin/icm", X_OK) != 0) {
            fprintf(stdout, "Warning: icm executable not found in PATH\n");
        }
        char *cwd;
        if (getcwd(cwd = malloc(1024), 1024) == NULL) {
            perror("getcwd");
            free(cwd);
            return 1;
        }
        char *path_working = access("/usr/bin/icm", X_OK) == 0 ? "/usr/bin/icm" : access("/bin/icm", X_OK) == 0 ? "/bin/icm" : (strcat(cwd, "/dist/icm"), cwd);
        fprintf(stdout, "I will now execute icm from %s\n", path_working);
        if (strcmp(path_working, "/usr/bin/icm") != 0 && strcmp(path_working, "/bin/icm") != 0) {
            fprintf(stderr, "Warning: Found icm executable at %s, but it's not in a standard location. Attempting to execute it anyway.\n", path_working);
        }
        execlp(path_working, path_working, "-b", "auto", "-S", path, NULL);
        perror("execlp");
        free(cwd);
        return 1;
    }

    fprintf(stderr, "Unknown command: %s\n", argv[1]);
    return 1;
}
