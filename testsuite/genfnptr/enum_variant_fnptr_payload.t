//@ expect val 42
fn double[T](T x) T { return x + x }
fn inc[T](T x) T { return x + 1 }
enum OpKind { fn(i32) i32 Named  None }
fn main() i32 {
    OpKind o = .Named{double}
    match o { .Named{f} { return f(21) }  .None { return 0 } }
    return 0
}
