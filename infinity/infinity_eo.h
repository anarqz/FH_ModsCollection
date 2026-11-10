#ifndef INFINITY_EO_H
#define INFINITY_EO_H
#include <stddef.h>

/* Infinity build (INFINITY_EO): Spotify Radio as an ExtraOptions plugin, see infinity/README.md. */

// Around one jmp hook's write: every other thread of the game is suspended, and one stopped inside
// target..target+length continues at the same offset of the trampoline (the copied, position-independent
// prologue), so no thread runs half-written bytes. Plugins start while the game already runs.
void infinity_freeze_threads(const unsigned char *target, size_t length, const unsigned char *trampoline);
void infinity_thaw_threads(void);

#endif
