// [[Rcpp::depends(RcppParallel)]]
// [[Rcpp::plugins(cpp17)]]
#include <Rcpp.h>

#include "../../include/io/tbb_flow_compat.h"

// [[Rcpp::export]]
Rcpp::List qdata_tbb_flow_compat_probe() {
    tbb::flow::graph graph;
    int next_value = 0;
    int produced = 0;
    int consumed = 0;
    int sum = 0;

    qio::tbb_compat::source_node<int> source(graph,
    [&next_value, &produced](int & value) {
        if(next_value >= 3) {
            return false;
        }
        value = next_value++;
        ++produced;
        return true;
    });

    tbb::flow::limiter_node<int> limiter(graph, 1);
    tbb::flow::function_node<int, tbb::flow::continue_msg> sink(graph, tbb::flow::serial,
    [&consumed, &sum](int value) {
        ++consumed;
        sum += value;
        return tbb::flow::continue_msg{};
    });

    tbb::flow::make_edge(source, limiter);
    tbb::flow::make_edge(limiter, sink);
    tbb::flow::make_edge(sink, qio::tbb_compat::decrementer(limiter));

    source.activate();
    graph.wait_for_all();

    return Rcpp::List::create(
        Rcpp::Named("uses_input_node") = static_cast<bool>(QIO_TBB_FLOW_USES_INPUT_NODE),
        Rcpp::Named("produced") = produced,
        Rcpp::Named("consumed") = consumed,
        Rcpp::Named("sum") = sum
    );
}
