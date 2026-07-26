//@ use std/option.t std/iterator.t std/vector.t std/array.t std/hashmap.t
//@ expect stdout
//@ | lookup=100
//@ | len=2
//@ | nested=7
// REGRESSION: any type is a key. Struct keys hash field-by-field (not by raw
// bytes -- padding is not part of a value), and keys_equal must peel wherever
// hash_key peels: a struct containing a pointer field has no lanewise ==.
extern fn printf(u8* fmt, ...) i32
struct Pt { i32 x  i32 y }
struct Inner { i32 a  i32 b }
struct Outer { Inner i  u8* name }
fn main() i32 {
    HashMap[Pt, i32] m = HashMap[Pt, i32].create()
    Pt a = {.x = 1, .y = 2}
    Pt b = {.x = 1, .y = 2}
    Pt c = {.x = 9, .y = 9}
    m[a] = 100
    m[c] = 300
    printf("lookup=%d\n", m[b])
    printf("len=%d\n", (i32)m.len())
    HashMap[Outer, i32] n = HashMap[Outer, i32].create()
    Outer k1 = {.i = {.a = 1, .b = 2}, .name = "x"}
    Outer k2 = {.i = {.a = 1, .b = 2}, .name = "x"}
    n[k1] = 7
    printf("nested=%d\n", n[k2])
    return 0
}
