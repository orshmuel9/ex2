#ifndef BUFFERED_OPEN_H
#define BUFFERED_OPEN_H

#include <fcntl.h>
#include <unistd.h>

// Define a new flag that doesn't collide with existing flags
#define O_PREAPPEND 0x40000000

// Structure to hold the buffer and original flags
typedef struct {
    int fd;
    char *buffer;
    size_t buffer_size;
    size_t buffer_pos;
    int flags;
    int preappend; // flag to remember if O_PREAPPEND was used
} buffered_file_t;

// Function to wrap the original open function
buffered_file_t *buffered_open(const char *pathname, int flags, ...);

// Function to write to the buffered file
ssize_t buffered_write(buffered_file_t *bf, const void *buf, size_t count);

// Function to read from the buffered file
ssize_t buffered_read(buffered_file_t *bf, void *buf, size_t count);

// Function to flush the buffer to the file
int buffered_flush(buffered_file_t *bf);

// Function to close the buffered file
int buffered_close(buffered_file_t *bf);

#endif // BUFFERED_OPEN_H