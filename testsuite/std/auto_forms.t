//@ use std/option.t std/iterator.t std/vector.t std/array.t std/hashmap.t
//@ expect stdout
//@ | vec=1
//@ | arr=7
//@ | map=42
//@ | destructure=3
// `auto` is a lexer alias for `unpack`, so `auto x = expr` is the single-binder
// case of an irrefutable destructure -- not a separate type-inference feature.
// That is why it also destructures, and why it is SAFER than C++'s auto: the
// same construct covers both, and anything refutable is rejected outright.
extern fn printf(u8* fmt, ...) i32
struct P { i32 a  i32 b }
fn main() i32 {
    auto v = Vector[i32].create()
    v.push(1)
    printf("vec=%d\n", v[0])
    auto a = Array[i32, 3].filled(7)
    printf("arr=%d\n", a[0])
    auto m = HashMap[i32, i32].create()
    m[9] = 42
    printf("map=%d\n", m[9])
    P p = {.a = 1, .b = 2}
    auto {x, y} = p
    printf("destructure=%d\n", x + y)
    return 0
}
