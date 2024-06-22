#include "copytree.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>

#define PATH_MAX 4096

void copy_file(const char *src, const char *dest, int copy_symlinks, int copy_permissions) {
    int src_fd, dest_fd;
    ssize_t bytes_read;
    char buffer[4096];
    struct stat src_stat;
    mode_t mode;

    // Handle symbolic links
    if (copy_symlinks) {
        if (lstat(src, &src_stat) == -1) {
            perror("lstat failed");
            exit(EXIT_FAILURE);
        }
        if (S_ISLNK(src_stat.st_mode)) {
            char link_target[PATH_MAX];
            ssize_t len = readlink(src, link_target, sizeof(link_target) - 1);
            if (len == -1) {
                perror("readlink failed");
                exit(EXIT_FAILURE);
            }
            link_target[len] = '\0';
            printf("Creating symlink %s -> %s\n", dest, link_target);
            if (symlink(link_target, dest) == -1) {
                perror("symlink failed");
                exit(EXIT_FAILURE);
            }
            return;
        }
    }

    // Open the source file
    src_fd = open(src, O_RDONLY);
    if (src_fd == -1) {
        perror("open failed");
        exit(EXIT_FAILURE);
    }

    // Get the file status
    if (fstat(src_fd, &src_stat) == -1) {
        perror("fstat failed");
        close(src_fd);
        exit(EXIT_FAILURE);
    }

    // Set the file mode
    mode = copy_permissions ? src_stat.st_mode : (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

    // Open/create the destination file
    dest_fd = open(dest, O_WRONLY | O_CREAT | O_TRUNC, mode);
    if (dest_fd == -1) {
        perror("open failed");
        close(src_fd);
        exit(EXIT_FAILURE);
    }

    // Copy the file content
    while ((bytes_read = read(src_fd, buffer, sizeof(buffer))) > 0) {
        if (write(dest_fd, buffer, bytes_read) != bytes_read) {
            perror("write failed");
            close(src_fd);
            close(dest_fd);
            exit(EXIT_FAILURE);
        }
    }

    // Check for read error
    if (bytes_read == -1) {
        perror("read failed");
        close(src_fd);
        close(dest_fd);
        exit(EXIT_FAILURE);
    }

    // Close the source file
    if (close(src_fd) == -1) {
        perror("close failed");
        close(dest_fd);
        exit(EXIT_FAILURE);
    }

    // Close the destination file
    if (close(dest_fd) == -1) {
        perror("close failed");
        exit(EXIT_FAILURE);
    }
}

void create_directory(const char *path, mode_t mode) {
    char tmp[PATH_MAX];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (tmp[len - 1] == '/') {
        tmp[len - 1] = '\0';
    }

    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, mode) == -1 && errno != EEXIST) {
                perror("mkdir failed");
                fprintf(stderr, "Failed to create directory: %s\n", tmp);
                exit(EXIT_FAILURE);
            }
            *p = '/';
        }
    }
    if (mkdir(tmp, mode) == -1 && errno != EEXIST) {
        perror("mkdir failed");
        fprintf(stderr, "Failed to create directory: %s\n", tmp);
        exit(EXIT_FAILURE);
    }
}

void copy_directory(const char *src, const char *dest, int copy_symlinks, int copy_permissions) {
    DIR *dir;
    struct dirent *entry;
    struct stat src_stat;
    char src_path[PATH_MAX];
    char dest_path[PATH_MAX];

    // Open the source directory
    dir = opendir(src);
    if (dir == NULL) {
        perror("opendir failed");
        exit(EXIT_FAILURE);
    }

    // Create the destination directory
    create_directory(dest, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

    // Read the source directory
    errno = 0;  // Reset errno before reading directory entries
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        snprintf(src_path, sizeof(src_path), "%s/%s", src, entry->d_name);
        snprintf(dest_path, sizeof(dest_path), "%s/%s", dest, entry->d_name);

        if (lstat(src_path, &src_stat) == -1) {
            perror("lstat failed");
            closedir(dir); // Ensure directory is closed
            exit(EXIT_FAILURE);
        }

        if (S_ISDIR(src_stat.st_mode)) {
            copy_directory(src_path, dest_path, copy_symlinks, copy_permissions);
        } else {
            copy_file(src_path, dest_path, copy_symlinks, copy_permissions);
        }
    }

    // Check for readdir error
    if (errno != 0) {
        perror("readdir failed");
        closedir(dir); // Ensure directory is closed
        exit(EXIT_FAILURE);
    }

    // Close the directory
    if (closedir(dir) == -1) {
        perror("closedir failed");
        exit(EXIT_FAILURE);
    }
}
