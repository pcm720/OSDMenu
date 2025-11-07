#ifndef _PATINFO_H_
#define _PATINFO_H_

#include "loader.h"

// Processes partition attribute area into LoadOptions for the loader
// Will set the titleID argument to parsed titleID if titleID is not NULL
LoadOptions *parsePATINFO(int argc, char *argv[], char **titleID, int *disableHistory);

#endif
