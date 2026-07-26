//@ use std/option.t std/iterator.t std/vector.t std/closure.t
//@ expect stdout
//@ | same body two capture types: 84 5
//@ | mutation seen through ref: 4
//@ | closure capturing a closure: 30
// The remaining C++ lambda capabilities, again using ONLY std/closure.t.
//
// GENERIC LAMBDA (C++14 `[](auto x){...}`): one body reused at unrelated
// capture types, each instantiation separate and statically typed. C++ needed a
// dedicated language feature for this; here it falls out of `T... args` -- the
// pack IS the type constructor, so genericity and type-derivation are one act.
extern fn printf(u8* fmt, ...) i32
fn dbl[T](T... a) i32 { unpack {x} = a  return x + x }
fn dblf[T](T... a) f64 { unpack {x} = a  return x + x }
fn plen[T](T... a) i32 { unpack {p} = a  return (i32)p.len() }
fn callinner[T](T... a) i32 { unpack {inner} = a  return inner() }
fn add[T](T... a) i32 { unpack {x, y} = a  return x + y }
fn main() i32 {
    i32 a = 42
    f64 b = 2.5
    printf("same body two capture types: %d %d\n",
           closure(dbl, a)(), (i32)closure(dblf, b)())
    // [&] semantics: the closure sees later mutations through the pointer
    Vector[u8] v = Vector[u8].create()
    v.push(1)  v.push(2)  v.push(3)
    auto ref = closure(plen, &v)
    v.push(4)
    printf("mutation seen through ref: %d\n", ref())
    // a closure capturing another closure BY VALUE (Fn owns nothing)
    auto inner = closure(add, 10, 20)
    printf("closure capturing a closure: %d\n", closure(callinner, inner)())
    return 0
}
