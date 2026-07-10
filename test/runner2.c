/* The second half of the regression suite.
**
** runner.c holds every test; the library plus all of them no longer fit
** in one PRG. This selects the other half and reuses the same source,
** so no test or helper is ever duplicated. See the note at the top of
** runner.c.
*/
#define SUITE 2
#include "runner.c"
