//@ expect val 13
// `fn f[Ts...]()` and `fn g[T](T... args)` are the SAME construction applied to
// two different argument lists: the first bundles TYPE arguments written in
// brackets, the second bundles VALUE arguments written in parens. Both produce
// one ordinary anonymous-struct type parameter, so the pack-tail peel works on
// a function's own type-argument list exactly as it does on a struct's.
fn tsize[Ts...]() u32 {
    match Ts {
        struct { H; Rest... } { return (u32)sizeof(H) + tsize[Rest]() }
        else                  { return 0 }
    }
}
fn main() i32 { return (i32)tsize[i32, u8, f64]() }  // 4 + 1 + 8
