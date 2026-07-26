//@ use std/option.t std/iterator.t std/closure.t
//@ expect stdout
//@ | ints=30
//@ | floats=3
//@ | strs=Alice
// A closure is generic by default: nothing at the creation site declares a type
// parameter, yet the SAME `closure` and the SAME `Fn[T, R]` serve every capture
// shape. Genericity is not a closure feature -- it falls out of `T... args`,
// whose bundle type IS whatever was passed.
extern fn printf(u8* fmt, ...) i32
fn addi[T](T... a) i32 { unpack {x, y} = a  return x + y }
fn addf[T](T... a) f64 { unpack {x, y} = a  return x + y }
fn name[T](T... a) u8* { unpack {s} = a  return s }
fn main() i32 {
    auto ci = closure(addi, 10, 20)
    printf("ints=%d\n", ci())
    auto cf = closure(addf, 1.5, 1.5)
    printf("floats=%d\n", (i32)cf())
    auto cs = closure(name, "Alice")
    printf("strs=%s\n", cs())
    return 0
}
