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
#ifndef GAZER_TOOLS_GAZERTHETA_LIB_THETATYPE_H
#define GAZER_TOOLS_GAZERTHETA_LIB_THETATYPE_H

#include "gazer/Core/Type.h"

#include <string>

namespace gazer::theta
{

std::string thetaType(Type& type);
std::string defaultValueForType(Type& type);

} // end namespace gazer::theta

#endif // GAZER_TOOLS_GAZERTHETA_LIB_THETATYPE_H
