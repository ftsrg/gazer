#ifndef _GAZER_SUPPORT_GRAPHWRITER_H
#define _GAZER_SUPPORT_GRAPHWRITER_H

namespace gazer
{

template<class GraphT>
struct GraphTraits
{
    using NodeT = void;
    using EdgeT = void;

    using child_iterator  = void;
    using parent_iterator = void;

    using edge_iterator = void;
};


}

#endif
