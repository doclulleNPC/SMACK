// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id: version.c,v 1.2 1998/05/03 22:59:31 killough Exp $
//
//-----------------------------------------------------------------------------

static const char rcsid[] = "$Id: version.c,v 1.2 1998/05/03 22:59:31 killough Exp $";

#include "version.h"

int VERSION = 321;        // sf: made int from define 
const char version_date[] = __DATE__;
// SMACK! fork version, independent of the SMMU 3.21 base above.
// Bumped automatically on commit -- see tools/bump-version.sh and
// tools/hooks/pre-commit. Keep the literal on one line and in this exact
// shape; the bump script rewrites it with sed.
const char smack_version[] = "0.13.0";

const char version_name[] = "christmas"; // sf : version names
                                         // at the suggestion of mystican

//----------------------------------------------------------------------------
//
// $Log: version.c,v $
// Revision 1.2  1998/05/03  22:59:31  killough
// beautification
//
// Revision 1.1  1998/02/02  13:21:58  killough
// version information files
//
//----------------------------------------------------------------------------
