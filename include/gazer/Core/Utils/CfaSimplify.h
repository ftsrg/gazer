#ifndef _GAZER_CORE_UTILS_CFASIMPLIFY_H
#define _GAZER_CORE_UTILS_CFASIMPLIFY_H

#include "gazer/Core/Automaton.h"

namespace gazer
{

/**
 * Simplifies the input automaton, (hopefully) reducing its size.
 * This mostly means two steps:
 *  (1) Merging sequential edge sequences into one.
 *  (2) Removing unneeded locations.
 */
void SimplifyCFA(Automaton& cfa);

}

#endif
