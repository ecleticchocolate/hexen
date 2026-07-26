//@ use std/option.t std/iterator.t std/closure.t
//@ expect stdout
//@ | inferred=30
//@ | explicit=30
//@ | named=30
// Three ways to write the same closure. Genericity is the DEFAULT -- the first
// line names no type at all -- but it can be pinned two ways when a signature
// needs to be explicit (a struct field, a function parameter, a thread handle):
//
//   1. inferred   -- `auto c = closure(f, 10, 20)`
//   2. explicit   -- type arguments at the creation site
//   3. named      -- spell the Fn type out
//
// Note the bundle is a POSITIONAL anon struct: `struct{i32; i32}`, not
// `struct{i32 a  i32 b}`. Field names are part of a struct's identity here, and
// `T... args` synthesizes an unnamed one -- writing names gives a DIFFERENT
// type and a mismatch error that says so.
extern fn printf(u8* fmt, ...) i32
fn addi[T](T... a) i32 { unpack {x, y} = a  return x + y }
fn main() i32 {
    auto c1 = closure(addi, 10, 20)
    printf("inferred=%d\n", c1())
    auto c2 = closure[Args[i32, i32], i32](addi, 10, 20)
    printf("explicit=%d\n", c2())
    Fn[Args[i32, i32], i32] c3 = closure(addi, 10, 20)
    printf("named=%d\n", c3())
    return 0
}
