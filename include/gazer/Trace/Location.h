#ifndef _GAZER_TRACE_LOCATION_H
#define _GAZER_TRACE_LOCATION_H

#include <string>

namespace gazer
{

class LocationInfo
{
public:
    LocationInfo(unsigned int line = 0, unsigned int column = 0, std::string fileName = "")
        : mLine(line), mColumn(column), mFileName(fileName)
    {}

    LocationInfo(const LocationInfo&) = default;
    LocationInfo& operator=(const LocationInfo&) = default;

    int getLine() const { return mLine; }
    int getColumn() const { return mColumn; }
    std::string getFileName() const { return mFileName; }

private:
    int mLine;
    int mColumn;
    std::string mFileName;
};

}

#endif