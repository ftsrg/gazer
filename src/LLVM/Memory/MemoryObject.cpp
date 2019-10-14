#include "gazer/LLVM/Memory/MemoryObject.h"

using namespace gazer;

// LLVM pass implementation
//-----------------------------------------------------------------------------

char MemoryObjectPass::ID;

bool MemoryObjectPass::runOnFunction(llvm::Function& module)
{
    return false;
}

MemoryObject* MemorySSABuilder::createMemoryObject(
    MemoryAllocType allocType,
    MemoryObjectType objectType,
    MemoryObject::MemoryObjectSize size,
    llvm::Type* valueType,
    llvm::StringRef name
) {
    auto& ptr= mObjects.emplace_back(std::make_unique<MemoryObject>(
        mId++, allocType, objectType, size, valueType, name
    ));

    return &*ptr;
}

MemoryObject::~MemoryObject()
{}

void MemoryObject::print(llvm::raw_ostream& os) const
{
    os << "(" << mId << (mName.empty() ? "" : ", \"" + mName + "\"") << ", allocType=";
    switch (mAllocType) {
        case MemoryAllocType::Unknown: os << "Unknown"; break;
        case MemoryAllocType::Global: os << "Global"; break;
        case MemoryAllocType::Alloca: os << "Alloca"; break;
        case MemoryAllocType::Heap: os << "Heap"; break;
    }

    os << ", objectType=";
    switch (mObjectType) {
        case MemoryObjectType::Unknown: os << "Unknown"; break;
        case MemoryObjectType::Scalar: os << "Scalar"; break;
        case MemoryObjectType::Array: os << "Array"; break;
        case MemoryObjectType::Struct: os << "Struct"; break;
    }
    os << ", size=";
    if (mSize == UnknownSize) {
        os << "Unknown";
    } else {
        os << mSize;
    }

    os << ")";
}

void MemoryObject::addDefinition(MemoryObjectDef* def)
{

}

memory::EntryDef* MemorySSABuilder::getEntryDefinition(MemoryObject* object)
{
    return nullptr;
}

memory::StoreDef* MemorySSABuilder::addDefinition(MemoryObject* object, llvm::StoreInst& inst)
{
    auto def = new memory::StoreDef(object, object->mDefs.size(), inst);
    object->addDefinition(def);

    return def;
}
