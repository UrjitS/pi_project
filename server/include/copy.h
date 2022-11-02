#ifndef OPEN_COPY_H
#define OPEN_COPY_H

#include <unistd.h>

void copy(int from_fd, int to_fd, size_t count);

#endif //OPEN_COPY_H
