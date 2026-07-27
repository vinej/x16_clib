/* The third slice of the regression suite: the bitmap engine family.
**
** runner.c holds every test; the library plus all of them no longer fit
** in one PRG. This selects the engines batch and reuses the same
** source, so no test or helper is ever duplicated. See the note at the
** top of runner.c.
*/
#define SUITE 3
#include "runner.c"
