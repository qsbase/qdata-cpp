#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 1 ]; then
    echo "usage: $0 <conda-executable>" >&2
    exit 2
fi

conda_executable=$1

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
envs_dir="$script_dir/.conda-envs"
pkgs_dir="$script_dir/.conda-pkgs"
setup_tag="qdata-flow-compat-v1"

hash_file() {
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$1" | awk '{print $1}'
    elif command -v shasum >/dev/null 2>&1; then
        shasum -a 256 "$1" | awk '{print $1}'
    else
        echo "missing sha256 tool" >&2
        exit 2
    fi
}

has_conda_meta() {
    local env_prefix=$1
    local package_name=$2

    compgen -G "$env_prefix/conda-meta/$package_name-*.json" >/dev/null
}

capture_versions() {
    local env_prefix=$1

    CONDA_PKGS_DIRS="$pkgs_dir" "$conda_executable" run --prefix "$env_prefix" \
        Rscript -e 'pkgs <- c("Rcpp", "RcppParallel"); stopifnot(all(vapply(pkgs, requireNamespace, logical(1), quietly = TRUE))); cat(as.character(packageVersion("Rcpp")), "\n", as.character(packageVersion("RcppParallel")), "\n", sep = "")'
}

write_stamp() {
    local env_kind=$1
    local source_kind=$2
    local env_prefix="$envs_dir/$env_kind"
    local env_file="$script_dir/environment-$env_kind.yml"
    local stamp_file="$env_prefix/.qdata-flow-compat-stamp"
    local versions
    local rcpp_version
    local rcppparallel_version

    versions=$(capture_versions "$env_prefix")
    rcpp_version=$(printf '%s\n' "$versions" | sed -n '1p')
    rcppparallel_version=$(printf '%s\n' "$versions" | sed -n '2p')

    cat > "$stamp_file" <<EOF
STAMP_ENV_HASH=$(hash_file "$env_file")
STAMP_SETUP_TAG=$setup_tag
STAMP_SOURCE_KIND=$source_kind
STAMP_RCPP_VERSION=$rcpp_version
STAMP_RCPPPARALLEL_VERSION=$rcppparallel_version
EOF
}

env_is_ready() {
    local env_kind=$1
    local source_kind=$2
    local env_prefix="$envs_dir/$env_kind"
    local env_file="$script_dir/environment-$env_kind.yml"
    local stamp_file="$env_prefix/.qdata-flow-compat-stamp"
    local versions
    local rcpp_version
    local rcppparallel_version

    if [ ! -x "$env_prefix/bin/Rscript" ] || [ ! -f "$stamp_file" ]; then
        return 1
    fi

    STAMP_ENV_HASH=
    STAMP_SETUP_TAG=
    STAMP_SOURCE_KIND=
    STAMP_RCPP_VERSION=
    STAMP_RCPPPARALLEL_VERSION=
    # shellcheck disable=SC1090
    . "$stamp_file"

    if [ "$STAMP_ENV_HASH" != "$(hash_file "$env_file")" ]; then
        return 1
    fi
    if [ "$STAMP_SETUP_TAG" != "$setup_tag" ]; then
        return 1
    fi
    if [ "$STAMP_SOURCE_KIND" != "$source_kind" ]; then
        return 1
    fi

    case "$source_kind" in
        conda)
            has_conda_meta "$env_prefix" r-rcppparallel || return 1
            ;;
        cran)
            if has_conda_meta "$env_prefix" r-rcppparallel; then
                return 1
            fi
            ;;
        *)
            echo "unexpected source kind: $source_kind" >&2
            exit 2
            ;;
    esac

    if [ "$env_kind" = mixed ]; then
        has_conda_meta "$env_prefix" tbb-devel || return 1
    fi

    if ! versions=$(capture_versions "$env_prefix"); then
        return 1
    fi

    rcpp_version=$(printf '%s\n' "$versions" | sed -n '1p')
    rcppparallel_version=$(printf '%s\n' "$versions" | sed -n '2p')

    [ "$rcpp_version" = "$STAMP_RCPP_VERSION" ] || return 1
    [ "$rcppparallel_version" = "$STAMP_RCPPPARALLEL_VERSION" ] || return 1
}

rebuild_env() {
    local env_kind=$1
    local env_file="$script_dir/environment-$env_kind.yml"
    local env_prefix="$envs_dir/$env_kind"
    local log_file

    rm -rf "$env_prefix"
    mkdir -p "$envs_dir" "$pkgs_dir"
    log_file=$(mktemp)

    if CONDA_PKGS_DIRS="$pkgs_dir" "$conda_executable" env create --prefix "$env_prefix" --file "$env_file" --yes 2>&1 | tee "$log_file"; then
        rm -f "$log_file"
        return
    fi

    if grep -Eiq 'SafetyError|corrupt|incorrect size' "$log_file"; then
        echo "Detected corrupt local conda package cache, clearing $pkgs_dir and retrying once"
        rm -rf "$pkgs_dir" "$env_prefix"
        mkdir -p "$pkgs_dir"
        CONDA_PKGS_DIRS="$pkgs_dir" "$conda_executable" env create --prefix "$env_prefix" --file "$env_file" --yes
        rm -f "$log_file"
        return
    fi

    rm -f "$log_file"
    return 1
}

install_cran_rcppparallel() {
    local env_kind=$1
    local env_prefix="$envs_dir/$env_kind"

    CONDA_PKGS_DIRS="$pkgs_dir" "$conda_executable" run --prefix "$env_prefix" Rscript -e 'options(repos = c(CRAN = "https://cloud.r-project.org")); install.packages("RcppParallel", type = "source")'
}

ensure_env() {
    local env_kind=$1
    local source_kind=$2

    if env_is_ready "$env_kind" "$source_kind"; then
        echo "Reusing $env_kind env"
        return
    fi

    echo "Rebuilding $env_kind env"
    rebuild_env "$env_kind"
    if [ "$source_kind" = cran ]; then
        install_cran_rcppparallel "$env_kind"
    fi
    write_stamp "$env_kind" "$source_kind"
}

run_probe() {
    local env_kind=$1
    local env_prefix="$envs_dir/$env_kind"

    CONDA_PKGS_DIRS="$pkgs_dir" "$conda_executable" run --prefix "$env_prefix" Rscript "$script_dir/tbb_flow_compat.R" "$script_dir/tbb_flow_compat_probe.cpp"
}

echo "Testing RcppParallel from conda (oneTBB)"
ensure_env conda conda
run_probe conda

echo "Testing RcppParallel from CRAN (TBB 2019)"
ensure_env cran cran
run_probe cran

echo "Testing RcppParallel from CRAN with system tbb-devel (mixed header test)"
ensure_env mixed cran
run_probe mixed
