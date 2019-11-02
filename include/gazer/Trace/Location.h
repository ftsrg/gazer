//==-------------------------------------------------------------*- C++ -*--==//
//
// Copyright 2019 Contributors to the Gazer project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//===----------------------------------------------------------------------===//
#ifndef GAZER_TRACE_LOCATION_H
#define GAZER_TRACE_LOCATION_H

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