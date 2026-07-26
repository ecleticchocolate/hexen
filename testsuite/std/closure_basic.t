//@ use std/option.t std/iterator.t std/closure.t
//@ expect stdout
//@ | sum = 30
//@ | captures 2 fields sizeof=8 ret=4
//@ | Hello Alice, you are 30
// The demo that used to live in std/closure.t itself. A library module must not
// declare `main` or an extern the consumer also declares -- closure.t declared
// its own printf, which collided with any program that declared one too.
extern fn printf(u8* fmt, ...) i32
fn add[T](T... args) i32 {
    unpack {a, b} = args
    return a + b
}
fn greet[T](T... args) void {
    unpack {name, age} = args
    printf("Hello %s, you are %d\n", name, age)
}
// Static reasoning over captures -- possible only because nothing is erased.
fn describe[T, R](Fn[T, R] f) void {
    match T {
        struct { A; B } {
            printf("captures 2 fields sizeof=%d ret=%d\n", (i32)sizeof(T), (i32)sizeof(R))
        }
        else { printf("captures something else\n") }
    }
}
fn main() i32 {
    printf("sum = %d\n", closure(add, 10, 20)())
    auto fa = closure(add, 3, 4)
    describe(fa)
    closure(greet, "Alice", 30)()
    return 0
}
