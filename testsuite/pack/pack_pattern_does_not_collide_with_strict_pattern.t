//@ expect val 1233
fn strict2[T](T x) u32 {
    match T {
        struct { A; Rest } { return 1 }
        else { return 2 }
    }
}
fn peel2[T](T x) u32 {
    match T {
        struct { A; Rest... } { return 3 }
        else { return 4 }
    }
}
fn main() i32 {
    struct { i32 a  i32 b } v
    struct { i32 a  i32 b  i32 c } w
    return (i32)(strict2(v) * 1000 + strict2(w) * 100 + peel2(v) * 10 + peel2(w))
}
