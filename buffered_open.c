#include "buffered_open.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#define BUFFER_SIZE 4096

buffered_file_t *buffered_open(const char *pathname, int flags, ...) {
    buffered_file_t *bf = malloc(sizeof(buffered_file_t));
    if (!bf) {
        perror("malloc");
        return NULL;
    }

    if (flags & O_PREAPPEND) {
        bf->preappend = 1;
        flags &= ~O_PREAPPEND;
    } else {
        bf->preappend = 0;
    }

    bf->flags = flags;
    bf->buffer_size = BUFFER_SIZE;
    bf->buffer = malloc(bf->buffer_size);
    if (!bf->buffer) {
        perror("malloc buffer");
        free(bf);
        return NULL;
    }

    bf->buffer_pos = 0;

    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode_t mode = va_arg(args, mode_t);
        va_end(args);
        bf->fd = open(pathname, flags, mode);
    } else {
        bf->fd = open(pathname, flags);
    }

    if (bf->fd == -1) {
        perror("open");
        free(bf->buffer);
        free(bf);
        return NULL;
    }

     if (flags & O_TRUNC) {
        if (ftruncate(bf->fd, 0) == -1) {
            perror("ftruncate");
            close(bf->fd);
            free(bf->buffer);
            free(bf);
            return NULL;
        }
    }

    return bf;
}

ssize_t buffered_write(buffered_file_t *bf, const void *buf, size_t count) {
    size_t remaining_space = bf->buffer_size - bf->buffer_pos;

    while (count > 0) {
        if (remaining_space == 0) { 
            if (buffered_flush(bf) == -1) {
                return -1; 
            }
            remaining_space = bf->buffer_size; 
        }

        size_t bytes_to_write = (count < remaining_space) ? count : remaining_space;
        memcpy(bf->buffer + bf->buffer_pos, buf, bytes_to_write);
        bf->buffer_pos += bytes_to_write;
        buf = (const char *)buf + bytes_to_write; 
        count -= bytes_to_write; 
        remaining_space -= bytes_to_write; 
    }

    return 1;
}

ssize_t buffered_read(buffered_file_t *bf, void *buf, size_t count) {
    size_t bytes_to_read = count;
    size_t bytes_read = 0;
    char *buffer = buf;

    while (bytes_to_read > 0) {
        if (bf->buffer_pos == bf->buffer_size) {
            ssize_t result = read(bf->fd, bf->buffer, bf->buffer_size);
            if (result == -1) {
                perror("read");
                return -1; 
            } else if (result == 0) {
                break;
            }
            bf->buffer_size = result;
            bf->buffer_pos = 0;
        }
        size_t bytes_available = bf->buffer_size - bf->buffer_pos;
        size_t bytes_to_copy = bytes_to_read < bytes_available ? bytes_to_read : bytes_available;
        memcpy(buffer + bytes_read, bf->buffer + bf->buffer_pos, bytes_to_copy);
        bf->buffer_pos += bytes_to_copy;
        bytes_read += bytes_to_copy;
        bytes_to_read -= bytes_to_copy;
    }

    return bytes_read;
}

int buffered_flush(buffered_file_t *bf) {
    if (bf->buffer_pos > 0) {
        if (bf->preappend) {
            // Handle O_PREAPPEND: Read existing file content
            off_t current_offset = lseek(bf->fd, 0, SEEK_END);
            if (current_offset == -1) {
                perror("lseek end");
                return -1; 
            }

            char *temp_buffer = (char *)malloc(current_offset);
            if (!temp_buffer) {
                perror("malloc temp buffer");
                return -1; 
            }

            if (lseek(bf->fd, 0, SEEK_SET) == -1) {
                free(temp_buffer);
                perror("lseek set");
                return -1; 
            }

            ssize_t read_bytes = read(bf->fd, temp_buffer, current_offset);
            if (read_bytes == -1) {
                free(temp_buffer);
                perror("read");
                return -1; 
            }

            if (lseek(bf->fd, 0, SEEK_SET) == -1) {
                free(temp_buffer);
                perror("lseek set");
                return -1; 
            }

            ssize_t written = write(bf->fd, bf->buffer, bf->buffer_pos);
            if (written == -1) {
                free(temp_buffer);
                perror("write buffer");
                return -1; 
            }

            written = write(bf->fd, temp_buffer, read_bytes);
            if (written == -1) {
                free(temp_buffer);
                perror("write temp buffer");
                return -1; 
            }

            free(temp_buffer);
        }
        else if (bf->flags & O_APPEND) {
            // Ensure the file descriptor is at the end of the file before writing
            if (lseek(bf->fd, 0, SEEK_END) == -1) {
                perror("lseek end");
                return -1;
            }

            ssize_t written = write(bf->fd, bf->buffer, bf->buffer_pos);
            if (written == -1) {
                perror("write buffer");
                return -1; 
            }
        }
         else {    
            ssize_t written = write(bf->fd, bf->buffer, bf->buffer_pos);
            if (written == -1) {
                perror("write buffer");
                return -1; 
            }

            // // Truncate the file to remove any remaining old content
            // if (ftruncate(bf->fd, bf->buffer_pos) == -1) {
            //     perror("ftruncate");
            //     return -1;
            // }
        }
        bf->buffer_pos = 0; 
    }
    return 1;
}

int buffered_close(buffered_file_t *bf) {
    if (bf->flags & (O_WRONLY | O_RDWR)) {
        if (buffered_flush(bf) == -1) {
            return -1;
        }
    }
    int ret = close(bf->fd);
    if (ret == -1) {
        perror("close");
    }
    free(bf->buffer);
    free(bf);
    return ret;
}

int main() {
    buffered_file_t *bf = buffered_open("example1.txt", O_RDWR | O_CREAT | O_TRUNC,  0644);
    if (!bf) {
        perror("buffered_open");
        return 1;
    }

    const char *text = "Hello, World TRUNCs!\n";
    if (buffered_write(bf, text, strlen(text)) == -1) {
        perror("buffered_write");
        buffered_close(bf);
        return 1;
    }
    const char *text2 = "This is a test.\n";
    if (buffered_write(bf, text2, strlen(text2)) == -1) {
        perror("buffered_write");
        buffered_close(bf);
        return 1;
    }

    const char *text3 = "This is a test number 2 .\n";
    if (buffered_write(bf, text3, strlen(text3)) == -1) {
        perror("buffered_write");
        buffered_close(bf);
        return 1;
    }
    
    if (buffered_flush(bf) == -1) {
        perror("buffered_flush");
        buffered_close(bf);
        return 1;
    }

    if (buffered_close(bf) == -1) {
        perror("buffered_close");
        return 1;
    }

    char buffer[1024];
    buffered_file_t *bf2 = buffered_open("example1.txt", O_RDONLY);
    if (!bf2) {
        perror("buffered_open");
        return 1;
    }

    ssize_t read_bytes = buffered_read(bf2, buffer, sizeof(buffer));
    if (read_bytes == -1) {
        perror("buffered_read");
        buffered_close(bf2);
        return 1;
    }

    buffer[read_bytes] = '\0';
    printf("Read: %s", buffer);

    if (buffered_close(bf2) == -1) {
        perror("buffered_close");
        return 1;
    }
    

    

    return 0;
}
