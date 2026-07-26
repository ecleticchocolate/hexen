//@ use std/option.t std/iterator.t std/vector.t std/array.t std/hashmap.t
//@ expect stdout
//@ | len=4 sizeof=16
//@ | fill=7
//@ | set=1 val=9
//@ | oob=0
//@ | eq=1 neq=0
//@ | iter=30
extern fn printf(u8* fmt, ...) i32
fn main() i32 {
    Array[i32, 4] a
    printf("len=%d sizeof=%d\n", (i32)a.len(), (i32)sizeof(a))
    a.fill(7)
    printf("fill=%d\n", a[0])
    printf("set=%d val=%d\n", (i32)a.set(1, 9), a[1])
    printf("oob=%d\n", (i32)a.set(99, 0))
    Array[i32, 4] b = a.copy()
    printf("eq=%d neq=%d\n", (i32)(a == b), (i32)(a != b))
    i32 s = 0
    for i32* p in a { s = s + *p }
    printf("iter=%d\n", s)
    return 0
}
