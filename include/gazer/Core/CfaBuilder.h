#ifndef _GAZER_CORE_CFABUILDER_H
#define _GAZER_CORE_CFABUILDER_H

#include "gazer/Core/Automaton.h"

namespace gazer
{

class CfaBuilder
{
public:
    CfaBuilder();

    Location& createLocation(std::string name) {

    }

private:
    std::unique_ptr<Location> mEntry;
    std::unique_ptr<Location> mExit;
    std::vector<std::unique_ptr<Location>> mLocations;
};

}

#endif

