// print.c
//
// Linux and macOS. Straight to file descriptors 1 and 2, carrying on after short writes and
// interruptions.

#include "richc/print.h"

#include <errno.h>
#include <unistd.h>

// Write all of text to fd, or as much as the stream takes before failing.
static void rc_print_to(int fd, rc_str text)
{
    const char *at = text.data;
    uint32_t left = text.len;

    while (left > 0) {
        ssize_t written = write(fd, at, left);

        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }

            // Nowhere left to report it, so the text is lost
            return;
        }
        at += written;
        left -= (uint32_t)written;
    }
}


void rc_print(rc_str text)
{
    rc_print_to(STDOUT_FILENO, text);
}


void rc_print_err(rc_str text)
{
    rc_print_to(STDERR_FILENO, text);
}
