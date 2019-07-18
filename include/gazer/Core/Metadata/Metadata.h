#ifndef _GAZER_CORE_METADATA_METADATA_H
#define _GAZER_CORE_METADATA_METADATA_H

namespace gazer
{

class Metadata
{
public:

private:
};

class MyCustomMetadata : public Metadata
{
public:
    static constexpr char Name[] = "gazer.llvm.inlined_global_write";
};

class MetadataContainer
{
public:
    
};

} // end namespace gazer

#endif
