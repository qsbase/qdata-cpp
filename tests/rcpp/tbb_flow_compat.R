args <- commandArgs(TRUE)
stopifnot(length(args) <= 1L)
expected_api <- if (length(args) >= 1L) args[[1]] else ""

script_arg <- grep("^--file=", commandArgs(FALSE), value = TRUE)
stopifnot(length(script_arg) == 1L)
script_file <- normalizePath(sub("^--file=", "", script_arg), winslash = "/", mustWork = TRUE)
header_file <- normalizePath(
  file.path(dirname(script_file), "..", "..", "include", "io", "tbb_flow_compat.h"),
  winslash = "/",
  mustWork = TRUE
)

if (identical(expected_api, "")) {
  expected_uses_input_node <- NA
} else if (identical(expected_api, "source_node")) {
  expected_uses_input_node <- FALSE
} else if (identical(expected_api, "input_node")) {
  expected_uses_input_node <- TRUE
} else {
  stop("expected API must be one of '', 'source_node', or 'input_node'")
}

probe_source <- paste(
  "// [[Rcpp::depends(RcppParallel)]]",
  "// [[Rcpp::plugins(cpp17)]]",
  "#include <Rcpp.h>",
  "",
  sprintf("#include \"%s\"", header_file),
  "",
  "// [[Rcpp::export]]",
  "Rcpp::List qdata_tbb_flow_compat_probe() {",
  "    tbb::flow::graph graph;",
  "    int next_value = 0;",
  "    int produced = 0;",
  "    int consumed = 0;",
  "    int sum = 0;",
  "",
  "    qio::tbb_compat::source_node<int> source(graph,",
  "    [&next_value, &produced](int & value) {",
  "        if(next_value >= 3) {",
  "            return false;",
  "        }",
  "        value = next_value++;",
  "        ++produced;",
  "        return true;",
  "    });",
  "",
  "    tbb::flow::limiter_node<int> limiter(graph, 1);",
  "    tbb::flow::function_node<int, tbb::flow::continue_msg> sink(graph, tbb::flow::serial,",
  "    [&consumed, &sum](int value) {",
  "        ++consumed;",
  "        sum += value;",
  "        return tbb::flow::continue_msg{};",
  "    });",
  "",
  "    tbb::flow::make_edge(source, limiter);",
  "    tbb::flow::make_edge(limiter, sink);",
  "    tbb::flow::make_edge(sink, qio::tbb_compat::decrementer(limiter));",
  "",
  "    source.activate();",
  "    graph.wait_for_all();",
  "",
  "    return Rcpp::List::create(",
  "        Rcpp::Named(\"uses_input_node\") = static_cast<bool>(QIO_TBB_FLOW_USES_INPUT_NODE),",
  "        Rcpp::Named(\"produced\") = produced,",
  "        Rcpp::Named(\"consumed\") = consumed,",
  "        Rcpp::Named(\"sum\") = sum",
  "    );",
  "}",
  sep = "\n"
)

source_file <- tempfile("qdata-tbb-flow-compat-", fileext = ".cpp")
on.exit(unlink(source_file), add = TRUE)
writeLines(probe_source, source_file, useBytes = TRUE)

Rcpp::sourceCpp(
  file = source_file,
  rebuild = TRUE,
  showOutput = TRUE
)

result <- qdata_tbb_flow_compat_probe()

stopifnot(is.list(result))
stopifnot(is.logical(result$uses_input_node), length(result$uses_input_node) == 1L)
if (!is.na(expected_uses_input_node)) {
  stopifnot(identical(isTRUE(result$uses_input_node), expected_uses_input_node))
}
stopifnot(identical(as.integer(result$produced), 3L))
stopifnot(identical(as.integer(result$consumed), 3L))
stopifnot(identical(as.integer(result$sum), 3L))
