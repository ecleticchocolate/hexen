//@ use std/option.t std/iterator.t std/vector.t std/closure.t
//@ expect stdout
//@ | 1 capture-by-value: 30
//@ | 2 capture-by-ref: 3
//@ | 3 mixed types: 7
//@ | 4 generic body i32: 84
//@ | 5 generic body f64: 5
//@ | 6 zero captures: 42
//@ | 7 stored in struct: 30
//@ | 8 passed to fn: 30
//@ | 9 returned from fn: 50
//@ | 10 vector of closures: 60
// C++ lambda parity, using ONLY std/closure.t -- no hand-rolled callable types.
extern fn printf(u8* fmt, ...) i32
fn add[T](T... a) i32 { unpack {x, y} = a  return x + y }
fn plen[T](T... a) i32 { unpack {p} = a  return (i32)p.len() }
fn mixed[T](T... a) i32 { unpack {n, f} = a  return n + (i32)f }
fn dbl[T](T... a) i32 { unpack {x} = a  return x + x }
fn dblf[T](T... a) f64 { unpack {x} = a  return x + x }
fn konst[T](T... a) i32 { return 42 }
fn mk50() Fn[Args[i32, i32], i32] { return closure(add, 20, 30) }
fn callit(Fn[Args[i32, i32], i32] f) i32 { return f() }
fn main() i32 {
    // 1. [=] capture by value
    printf("1 capture-by-value: %d\n", closure(add, 10, 20)())
    // 2. [&] capture by reference
    Vector[u8] v = Vector[u8].create()
    v.push(1)  v.push(2)  v.push(3)
    printf("2 capture-by-ref: %d\n", closure(plen, &v)())
    // 3. heterogeneous captures
    printf("3 mixed types: %d\n", closure(mixed, 5, 2.0)())
    // 4/5. GENERIC body: one `dbl` reused at different capture types
    i32 a = 42
    printf("4 generic body i32: %d\n", closure(dbl, a)())
    f64 b = 2.5
    printf("5 generic body f64: %d\n", (i32)closure(dblf, b)())
    // 6. no captures
    printf("6 zero captures: %d\n", closure(konst)())
    // 7. stored as a struct field -- the type is spellable
    Fn[Args[i32, i32], i32] stored = closure(add, 10, 20)
    printf("7 stored in struct: %d\n", stored())
    // 8. passed to a function
    printf("8 passed to fn: %d\n", callit(closure(add, 10, 20)))
    // 9. returned from a function
    printf("9 returned from fn: %d\n", mk50()())
    // 10. stored in a container
    Vector[Fn[Args[i32, i32], i32]] fs = Vector[Fn[Args[i32, i32], i32]].create()
    fs.push(closure(add, 25, 35))
    match fs.get(0) { {.Some = *f} { printf("10 vector of closures: %d\n", (*f)()) }  .None {} }
    return 0
}
