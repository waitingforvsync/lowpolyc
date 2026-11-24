# common.sh
#
# Shared by the run.sh scripts beside it, which source it once they have changed to their own
# directory: where the library is, how to build a tool against the math sources, and where the
# outputs go. Not run by itself.
#
# Tools are compiled together with the transcendental sources, so no build of the library is
# needed. Set CC to choose the compiler (gcc by default; the libquadmath tools need gcc).

root=../../..
core=$root/src/richc/core
out=out
CC=${CC:-gcc}

# The flags the library is built with natively, plus what the tools need from libc.
cflags=(-O2 -ffp-contract=off -fno-math-errno -march=x86-64-v2 -std=c17 -D_GNU_SOURCE
    -I"$core/include" -I"$core/src")

# The transcendental sources: all of src/math but the matrices, quaternion, rational and solver.
math_sources=()
for source in "$core"/src/math/*.c; do
    case "${source##*/}" in
    mat*|quatf.c|rational.c|solve.c) ;;
    *) math_sources+=("$source") ;;
    esac
done

# build NAME TOOL.c [LIBS...]: compiles TOOL.c with the math sources into out/NAME.
build() {
    local name=$1
    local tool=$2
    shift 2
    mkdir -p "$out"
    "$CC" "${cflags[@]}" "$tool" "${math_sources[@]}" -o "$out/$name" "$@"
}

# compare ARGS...: runs compare.py, which checks generated tables and golden rows against the tree.
compare() {
    python3 ../compare.py "$@"
}
