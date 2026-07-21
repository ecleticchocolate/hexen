//@ expect val 12
// Name assertions nest: the inner slot asserts `q`, the outer asserts `inner`.
fn probe[T]() i32 {
    match T {
        struct { struct { i32 q } inner; u8 t } { return 1 }
        else { return 2 }
    }
    return -1
}
fn main() i32 {
    i32 hit  = probe[struct { struct { i32 q }  inner  u8 t }]()
    i32 miss = probe[struct { struct { i32 zz } inner  u8 t }]()
    return hit * 10 + miss
}
