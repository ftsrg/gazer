#ifndef _GAZER_CORE_METADATA_H
#define _GAZER_CORE_METADATA_H

#include <llvm/ADT/StringRef.h>

namespace gazer
{

class MetadataNode
{
public:
    virtual llvm::StringRef getMetadataName() const = 0;
};

class MetadataContainer
{
public:
    using md_iterator = std::vector<MetadataNode*>::iterator;
    md_iterator md_begin() { return mMetadataNodes.begin(); }
    md_iterator md_end() { return mMetadataNodes.end(); }

    ~MetadataContainer();
private:
    std::vector<MetadataNode*> mMetadataNodes;
};

class MetadataHandler
{
public:
    virtual MetadataNode* readNode(std::istream& is) = 0;
    virtual MetadataNode* writeNode(std::ostream& os) = 0;
};

} // end namespace gazer

#endif
