#ifndef _QIO_TBB_FLOW_COMPAT_H
#define _QIO_TBB_FLOW_COMPAT_H

#include <utility>
#include <tbb/flow_graph.h>

// TBB 2019 exposes only <tbb/flow_graph.h> and uses source_node/decrement.
// oneTBB ships its canonical flow-graph API under <oneapi/tbb/flow_graph.h>
// and uses input_node/decrementer(). Probe that header layout instead of
// version macros: some wrapper headers do not surface TBB_INTERFACE_VERSION,
// and mixing version headers from different packages can mis-detect the actual
// flow-graph API in use. Callers can override this probe if they know better.
#ifndef QIO_TBB_FLOW_USES_INPUT_NODE
#if defined(__has_include)
#if __has_include(<oneapi/tbb/flow_graph.h>)
#define QIO_TBB_FLOW_USES_INPUT_NODE 1
#else
#define QIO_TBB_FLOW_USES_INPUT_NODE 0
#endif
#else
#define QIO_TBB_FLOW_USES_INPUT_NODE 0
#endif
#endif

namespace qio {
namespace tbb_compat {

// Normalize the flow-graph API split between classic TBB (interface 11)
// and oneTBB (interface 12+).
//
// Classic TBB:
//   tbb::flow::source_node<T>
//   limiter_node.decrement
//   body signature: bool(T & out)
//
// oneTBB:
//   tbb::flow::input_node<T>
//   limiter_node.decrementer()
//   body signature: T(tbb::flow_control & fc)
//
// Normalized contract used below:
//   bool(T & out)
//
// Return true and fill `out` to emit one message.
// Return false to stop the node.
#if QIO_TBB_FLOW_USES_INPUT_NODE
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

template <class LimiterNode>
inline auto & decrementer(LimiterNode & node) {
    return node.decrementer();
}
#else
template <class T>
struct source_node : tbb::flow::source_node<T> {
    using base_type = tbb::flow::source_node<T>;

    template <class Body>
    source_node(tbb::flow::graph & graph, Body && body) :
    base_type(graph, std::forward<Body>(body), false) {}
};

template <class LimiterNode>
inline auto & decrementer(LimiterNode & node) {
    return node.decrement;
}
#endif

} // namespace tbb_compat
} // namespace qio

#endif
