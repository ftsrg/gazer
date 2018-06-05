#ifndef _GAZER_SUPPORT_LOGSTREAM_H
#define _GAZER_SUPPORT_LOGSTREAM_H

#include <llvm/Support/raw_ostream.h>

namespace gazer
{

class LogStream final : public llvm::raw_ostream
{
public:
};

}

#endif
