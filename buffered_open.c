#include "buffered_open.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#define BUFFER_SIZE 4096
int counterPreappend = 0;
// Function to open a buffered file
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

// Function to write to the buffered file
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
    if(remaining_space == 0) {
        if (buffered_flush(bf) == -1) {
            return -1; 
        }
    }
    return 1;
}

// Function to read from the buffered file
ssize_t buffered_read(buffered_file_t *bf, void *buf, size_t count) {
    size_t bytes_to_read = count;
    size_t bytes_read = 0;
    char *buffer = buf;

    // First read from the buffer if there is any data left
    if (bf->buffer_pos > 0) {
        size_t bytes_available = bf->buffer_pos < bytes_to_read ? bf->buffer_pos : bytes_to_read;
        memcpy(buffer, bf->buffer, bytes_available);
        bytes_read += bytes_available;
        bytes_to_read -= bytes_available;
        buffer += bytes_available;
    }

    // If there is still data to read, read from the file
    if (bytes_to_read > 0) {
        // Create a temporary buffer to store the current content
        char temp_buffer[BUFFER_SIZE];
        size_t temp_buffer_pos = bf->buffer_pos;
        memcpy(temp_buffer, bf->buffer, bf->buffer_pos);

        // Read from the file
        while (bytes_to_read > 0) {
            ssize_t result = read(bf->fd, bf->buffer, bf->buffer_size);
            if (result == -1) {
                perror("read");
                return -1;
            } else if (result == 0) { // End of file
                break;
            }
            size_t bytes_available = result < bytes_to_read ? result : bytes_to_read;
            memcpy(buffer, bf->buffer, bytes_available);
            bytes_read += bytes_available;
            bytes_to_read -= bytes_available;
            buffer += bytes_available;
        }

        // Restore the buffer content to its original state
        memcpy(bf->buffer, temp_buffer, temp_buffer_pos);
        bf->buffer_pos = temp_buffer_pos;
    }

    return bytes_read;
}


// Function to flush the buffered file
int buffered_flush(buffered_file_t *bf) {
    if (bf->buffer_pos > 0) {
        if (bf->preappend && counterPreappend == 0) {
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

            off_t offset = lseek(bf->fd, 0, SEEK_CUR);

            ssize_t write2 = write(bf->fd, temp_buffer, read_bytes);
            if (write2 == -1) {
                free(temp_buffer);
                perror("write temp buffer");
                return -1; 
            }

            if (lseek(bf->fd, offset, SEEK_SET) == -1) {
                perror("lseek set");
                return -1;
            }

            free(temp_buffer);
            counterPreappend++;
        } else if (bf->preappend && counterPreappend > 0) {
            // Save current position
            off_t saved_offset = lseek(bf->fd, 0, SEEK_CUR);

            // Move to the end to find out the size of the remaining content
            off_t current_offset = lseek(bf->fd, 0, SEEK_END);
            if (current_offset == -1) {
                perror("lseek end");
                return -1; 
            }

            // Calculate the size of remaining content
            off_t remaining_size = current_offset - saved_offset;
            char *temp_buffer = (char *)malloc(remaining_size);
            if (!temp_buffer) {
                perror("malloc temp buffer");
                return -1; 
            }

            // Read the remaining content into the temp buffer
            if (lseek(bf->fd, saved_offset, SEEK_SET) == -1) {
                free(temp_buffer);
                perror("lseek set");
                return -1; 
            }

            ssize_t read_bytes = read(bf->fd, temp_buffer, remaining_size);
            if (read_bytes == -1) {
                free(temp_buffer);
                perror("read");
                return -1; 
            }

            // Move back to the saved position and write the buffer
            if (lseek(bf->fd, saved_offset, SEEK_SET) == -1) {
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

            // Write the remaining content back
            ssize_t write2 = write(bf->fd, temp_buffer, read_bytes);
            if (write2 == -1) {
                free(temp_buffer);
                perror("write temp buffer");
                return -1; 
            }

            free(temp_buffer);

            // Move the file offset to just after the written buffer, before the old content
            if (lseek(bf->fd, saved_offset + written, SEEK_SET) == -1) {
                perror("lseek set");
                return -1;
            }
        } else if (bf->flags & O_APPEND) {
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
        } else {    
            ssize_t written = write(bf->fd, bf->buffer, bf->buffer_pos);
            if (written == -1) {
                perror("write buffer");
                return -1; 
            }
        }
        bf->buffer_pos = 0; 
    }
    return 1;
}

// Function to close the buffered file
int buffered_close(buffered_file_t *bf) {
    if (bf->flags & (O_WRONLY | O_RDWR)) { // Only flush if opened in write mode
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
    buffered_file_t *bf = buffered_open("example.txt", O_RDWR | O_CREAT | O_PREAPPEND, 0644);
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

    // Move the file descriptor to the beginning of the file before reading
    // if (lseek(bf->fd, 0, SEEK_SET) == -1) {
    //     perror("lseek set");
    //     buffered_close(bf);
    //     return 1;
    // }

    const char *text4 = "This is a test number 3 .\n";
    if (buffered_write(bf, text4, strlen(text4)) == -1) {
        perror("buffered_write");
        buffered_close(bf);
        return 1;
    }

    if(buffered_flush(bf) == -1) {
        perror("buffered_flush");
        buffered_close(bf);
        return 1;
    }

    const char *text5 = "This is a test number 4 .\n";
    if (buffered_write(bf, text5, strlen(text5)) == -1) {
        perror("buffered_write");
        buffered_close(bf);
        return 1;
    }


    

    // char buffer2[1024];
    // ssize_t read_bytes2 = buffered_read(bf, buffer2, sizeof(buffer2) - 1);
    // if (read_bytes2 == -1) {
    //     perror("buffered_read");
    //     buffered_close(bf);
    //     return 1;
    // }
    // buffer2[read_bytes2] = '\0';
    // printf("Read: %s\n", buffer2);

    if (buffered_close(bf) == -1) {
        perror("buffered_close");
        return 1;
    }
    

    // Additional read from a new file descriptor
    char buffer[1024];
    buffered_file_t *bf2 = buffered_open("example1.txt", O_RDONLY);
    if (!bf2) {
        perror("buffered_open");
        return 1;
    }

    ssize_t read_bytes = buffered_read(bf2, buffer, sizeof(buffer) - 1);
    if (read_bytes == -1) {
        perror("buffered_read");
        buffered_close(bf2);
        return 1;
    }
    buffer[read_bytes] = '\0';
    printf("Read again: %s", buffer);


    if (buffered_close(bf2) == -1) {
        perror("buffered_close");
        return 1;
    }

    return 0;
}
