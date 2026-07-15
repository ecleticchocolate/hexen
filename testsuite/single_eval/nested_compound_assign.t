//@ expect val 161511
// A compound assign's RHS containing ANOTHER compound assign on a complex
// lvalue exercises the compile_lvalue memo's save/restore: the inner
// a[outer_idx()] += (...) must not have its spilled address entry clobbered
// or left dangling by the nested b[inner_idx()] += 5 compiling in between.
i32 outer_calls = 0
i32 inner_calls = 0
fn outer_idx() i32 {
    outer_calls = outer_calls + 1
    return outer_calls - 1
}
fn inner_idx() i32 {
    inner_calls = inner_calls + 1
    return inner_calls - 1
}
fn main() i32 {
    i32[3] a = {1, 2, 3}
    i32[3] b = {10, 20, 30}
    a[outer_idx()] += (b[inner_idx()] += 5)
    return a[0]*10000 + b[0]*100 + outer_calls*10 + inner_calls
}
