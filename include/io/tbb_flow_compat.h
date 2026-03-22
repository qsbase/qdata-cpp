#ifndef _QIO_TBB_FLOW_COMPAT_H
#define _QIO_TBB_FLOW_COMPAT_H

#include <utility>
#if __has_include(<tbb/version.h>)
#include <tbb/version.h>
#elif __has_include(<oneapi/tbb/version.h>)
#include <oneapi/tbb/version.h>
#endif
#include <tbb/flow_graph.h>

namespace qio {
namespace tbb_compat {

// Normalize old TBB flow-graph source APIs across versions.
//
// Old interface:
//   tbb::flow::source_node<T>
//   body signature: bool(T & out)
//
// Newer interface:
//   tbb::flow::input_node<T>
//   body signature: T(tbb::flow_control & fc)
//
// Normalized contract used below:
//   bool(T & out)
//
// Return true and fill `out` to emit one message.
// Return false to stop the node.
#if defined(TBB_INTERFACE_VERSION) && TBB_INTERFACE_VERSION >= 11102
template <class T>
struct source_node : tbb::flow::input_node<T> {
    using base_type = tbb::flow::input_node<T>;

    template <class Body>
    source_node(tbb::flow::graph & graph, Body && body) :
    base_type(graph,
    [body = std::forward<Body>(body)](tbb::flow_control & fc) mutable -> T {
        T out{};
        if(!body(out)) {
            fc.stop();
        }
        return out;
    }) {}
};
#else
template <class T>
struct source_node : tbb::flow::source_node<T> {
    using base_type = tbb::flow::source_node<T>;

    template <class Body>
    source_node(tbb::flow::graph & graph, Body && body) :
    base_type(graph, std::forward<Body>(body), false) {}
};
#endif

} // namespace tbb_compat
} // namespace qio

#endif
