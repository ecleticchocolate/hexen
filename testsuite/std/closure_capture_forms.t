//@ use std/option.t std/iterator.t std/vector.t std/closure.t
//@ expect stdout
//@ | scalar=30
//@ | ptr=2
//@ | mixed=12
//@ | zero=7
// Every legitimate capture shape must still compile: scalars, pointers to
// owning values, a mix of both, and no captures at all. The guard walks the
// whole bundle, so an owning value in ANY position is caught -- and a pointer
// in any position is fine.
extern fn printf(u8* fmt, ...) i32
fn add2[T](T... a) i32 { unpack {x, y} = a  return x + y }
fn plen[T](T... a) u32 { unpack {p} = a  return p.len() }
fn mix[T](T... a) i32 { unpack {n, p} = a  return n + (i32)p.len() }
fn none[T](T... a) i32 { return 7 }
fn main() i32 {
    printf("scalar=%d\n", closure(add2, 10, 20)())
    Vector[u8] v = Vector[u8].create()
    v.push(1)  v.push(2)
    printf("ptr=%d\n", (i32)closure(plen, &v)())
    printf("mixed=%d\n", closure(mix, 10, &v)())
    printf("zero=%d\n", closure(none)())
    return 0
}
