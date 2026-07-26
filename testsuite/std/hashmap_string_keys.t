//@ use std/option.t std/iterator.t std/vector.t std/array.t std/hashmap.t
//@ expect stdout
//@ | hello=1 world=2
//@ | len=2
// REGRESSION: u8* keys must hash and compare BY CONTENT. Two identical string
// literals are two different pointers, so an address-based map inserted a
// second entry per lookup and read back 0.
extern fn printf(u8* fmt, ...) i32
fn main() i32 {
    HashMap[u8*, i32] m = HashMap[u8*, i32].create()
    m["hello"] = 1
    m["world"] = 2
    printf("hello=%d world=%d\n", m["hello"], m["world"])
    printf("len=%d\n", (i32)m.len())
    return 0
}
