//@ use std/option.t std/iterator.t std/vector.t std/array.t std/hashmap.t
//@ expect stdout
//@ | scalar=1
//@ | string=1
//@ | struct=1
//@ | nested=1
// INVARIANT: hash_key and keys_equal must agree on every key shape. They are
// two separate walks (hash runs once per lookup, equality once per probe, so
// fusing them would hash on every probe), and nothing in the type system ties
// them together -- they HAVE drifted once: four-tier hashing shipped against
// one-tier equality, and a struct key holding a pointer silently failed.
//
// The law: equal keys must hash equal. Each line below inserts under one key
// and reads back under a DISTINCT but equal key -- which only succeeds if the
// hash sent both to the same slot AND equality recognised them there.
extern fn printf(u8* fmt, ...) i32
struct Pt { i32 x  i32 y }
struct In { i32 a  i32 b }
struct Out { In i  u8* name }
fn main() i32 {
    HashMap[i32, i32] a = HashMap[i32, i32].create()
    a[7] = 1
    printf("scalar=%d\n", a[7])

    HashMap[u8*, i32] b = HashMap[u8*, i32].create()
    u8* k1 = "key"
    u8* k2 = "key"
    b[k1] = 1
    printf("string=%d\n", b[k2])

    HashMap[Pt, i32] c = HashMap[Pt, i32].create()
    Pt p1 = {.x = 3, .y = 4}
    Pt p2 = {.x = 3, .y = 4}
    c[p1] = 1
    printf("struct=%d\n", c[p2])

    HashMap[Out, i32] d = HashMap[Out, i32].create()
    Out o1 = {.i = {.a = 1, .b = 2}, .name = "n"}
    Out o2 = {.i = {.a = 1, .b = 2}, .name = "n"}
    d[o1] = 1
    printf("nested=%d\n", d[o2])
    return 0
}
