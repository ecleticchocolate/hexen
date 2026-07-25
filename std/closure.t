extern fn printf(u8* fmt, ...) i32

// No erasure at all. T is whatever anonymous struct T... args already
// synthesizes (this is exactly "Fn[Args[A,B], C]" — T... already IS the
// Args[A,B] carrier, just anonymous instead of named). No heap, no delete.
struct Fn[T, R] { fn(T) R body; T captures }
impl Fn[T, R] { fn __call() R { return self.body(self.captures) } }

fn closure[T, R](fn(T) R body, T... args) Fn[T, R] {
    return { .body = body, .captures = args }
}

fn add[T](T... args) i32 {
    unpack {a, b} = args
    return a + b
}

fn greet[T](T... args) void {
    unpack {name, age} = args
    printf("Hello %s, you are %d\n", name, age)
}

// Static reasoning over captures: possible now, impossible when erased.
fn describe[T, R](Fn[T, R] f) void {
    match T {
        struct { A; B } {
            printf("closure captures 2 fields, sizeof=%d, returns sizeof=%d\n",
                   (i32)sizeof(T), (i32)sizeof(R))
        }
        else { printf("closure captures something else\n") }
    }
}

fn main() i32 {
    i32 sum = closure(add, 10, 20)()
    printf("sum = %d\n", sum)

    auto fa = closure(add, 3, 4)
    describe(fa)

    closure(greet, "Alice", 30)()
    return 0
}