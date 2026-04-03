args <- commandArgs(TRUE)
stopifnot(length(args) == 1L)

variant <- args[[1]]
repos <- c(CRAN = "https://cloud.r-project.org")
ncpus <- max(1L, parallel::detectCores())

if (identical(variant, "classic")) {
  Sys.unsetenv(c("TBB", "TBB_ROOT", "TBB_INC", "TBB_LIB", "TBB_LINK_LIB", "TBB_AUTODETECT"))
} else if (identical(variant, "onetbb")) {
  Sys.unsetenv("TBB_AUTODETECT")
  required <- c("TBB_INC", "TBB_LIB")
  missing <- required[!nzchar(Sys.getenv(required))]
  if (length(missing) > 0L) {
    stop("Missing required environment variables: ", paste(missing, collapse = ", "))
  }
} else {
  stop("Unknown variant: ", variant)
}

message("Installing RcppParallel variant: ", variant)
install.packages("Rcpp", repos = repos, Ncpus = ncpus)
install.packages("RcppParallel", repos = repos, type = "source", Ncpus = ncpus)

pkgs <- c("Rcpp", "RcppParallel")
stopifnot(all(vapply(pkgs, requireNamespace, logical(1), quietly = TRUE)))
