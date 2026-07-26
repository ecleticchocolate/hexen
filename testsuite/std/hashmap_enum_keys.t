//@ use std/option.t std/iterator.t std/vector.t std/array.t std/hashmap.t
//@ expect stdout
//@ | payload=10
//@ | nopayload=20
//@ | distinct=2
// An enum key hashes and compares through the SAME structural walk a struct
// uses -- an enum is a StructDef whose fields are its variants, so hash_key's
// `struct { H; Rest... }` arm reaches it with no enum-specific code.
extern fn printf(u8* fmt, ...) i32
enum Color { u32 Red  None }
fn main() i32 {
    HashMap[Color, i32] m = HashMap[Color, i32].create()
    Color a = {.Red = 1}
    Color b = {.Red = 1}
    Color n = .None
    m[a] = 10
    m[n] = 20
    printf("payload=%d\n", m[b])
    printf("nopayload=%d\n", m[n])
    printf("distinct=%d\n", (i32)m.len())
    return 0
}
