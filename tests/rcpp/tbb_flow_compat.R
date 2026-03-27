args <- commandArgs(TRUE)
source_file <- normalizePath(args[[1]], winslash = "/", mustWork = TRUE)

Rcpp::sourceCpp(
  file = source_file,
  rebuild = TRUE,
  showOutput = TRUE
)

result <- qdata_tbb_flow_compat_probe()

stopifnot(is.list(result))
stopifnot(identical(as.integer(result$produced), 3L))
stopifnot(identical(as.integer(result$consumed), 3L))
stopifnot(identical(as.integer(result$sum), 3L))
