#include <stdio.h>  
#include <stdlib.h>
#include <sys/types.h> 
#include <sys/wait.h>
#include <unistd.h>


int main(int argc ,char *argv[]) {
    if(argc != 5 ){
        fprintf(stderr, "Usage: %s <parent_message> <child1_message> <child2_message> <count>", argv[0]);
        return 1;
    }
    int count = atoi(argv[4]);
    char *parent_message = argv[1];
    char *child1_message = argv[2];
    char *child2_message = argv[3];
    FILE *file = fopen("output.txt", "w");
    if (file == NULL) {
        perror("fopen");
        return 1;
    }
    int pid = fork();
    if (pid < 0) {
        perror("fork failed");
        return 1;
    }
    if(pid == 0){
        for(int i = 0; i < count; i++){
            fprintf(file, "%s\n", child1_message);
        }
        fclose(file);
        exit (0);
    }else{
        wait(NULL);
        int pid2 = fork();
        if (pid2 < 0) {
            perror("fork failed");
            return 1;
        }
        if(pid2 == 0){
            sleep(1);
            for(int i = 0; i < count; i++){
                fprintf(file, "%s\n", child2_message);
            }
            fclose(file);
            exit (0);
        }
        else{
            wait(NULL);
            sleep(1);
            for(int i = 0; i < count; i++){
                fprintf(file, "%s\n", parent_message);
            }
            fclose(file);
            exit (0);
        }
    }
    fclose(file);
    return 0;
}