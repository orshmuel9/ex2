#include <stdio.h>  
#include <stdlib.h>
#include <sys/types.h> 
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

void write_message(const char *message, int count) {
    for (int i = 0; i < count; i++) {
        printf("%s\n", message);
        usleep((rand() % 100) * 1000); // Random delay between 0 and 99 milliseconds
    }
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <message1> <message2> ... <count>\n", argv[0]);
        return 1;
    }

    const char *lockfile = "lockfile.lock";
    unlink(lockfile);
    int num_times = atoi(argv[argc - 1]);
    int num_forks = argc - 2;

    pid_t pids[num_forks];
    // char *order = argv[argc - 2];
    // int real_order[num_forks];
    // char *token = strtok(order, " ");
    // for (int i = 0; i < num_forks; i++) {
    //     real_order[i] = atoi(token) - 1;
    //     token = strtok(NULL, " ");
    // }

    for (int i = 0; i < num_forks; i++) {
        pids[i] = fork();
        if (pids[i] < 0) {
            perror("fork failed");
            return 1;
        }
        if (pids[i] == 0) {
            const char *message = argv[i+1];
            while (1) {
                int fd = open(lockfile, O_CREAT | O_EXCL, 0666);
                if (fd != -1) {
                    close(fd);
                    break;
                    }
                    usleep(1000); // Wait 1 millisecond before retrying
            }
            // Write to stdout
            write_message(message, num_times);   
            fflush(stdout);  
            // Release lock
            unlink(lockfile);
            exit(0);
        }
        sleep(2);
    }
    // Parent process
    for (int i = 0; i < num_forks; i++) {
        waitpid(pids[i], NULL, 0);
    }
    return 0;
}
