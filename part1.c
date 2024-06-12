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
    FILE *file = fopen("output.txt", "w");
    int pid = fork();
    if (pid < 0) {
        fprintf(stderr, "Fork Failed");
        return 1;
    }
    if(pid == 0){
        int pid2 = fork();
        if (pid2 < 0) {
            fprintf(stderr, "Fork Failed");
            return 1;
        }
        if(pid2 == 0){
            for(int i = 0; i < atoi(argv[4]); i++){
                //printf("%s\n", argv[3]);
                fprintf(file, "%s\n", argv[2]);
            }
            fclose(file);
            sleep(1);
            exit (0);
        }else{
            for(int i = 0; i < atoi(argv[4]); i++){
                //printf("%s\n", argv[2]);
                fprintf(file, "%s\n", argv[3]);
            }
            fclose(file);
            wait(NULL);
            exit (0);
        }
    }else{
        wait(NULL);
        wait(NULL);
        for(int i = 0; i < atoi(argv[4]); i++){
            //printf("%s\n", argv[1]);
            fprintf(file, "%s\n", argv[1]);
        }
        exit (0);
    }
    fclose(file);
    return 0;
}