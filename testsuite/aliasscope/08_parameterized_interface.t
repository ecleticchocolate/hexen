//@ expect val 1
struct R { i32 v }
pub impl R { fn get() i32 { return 5 } }
alias Getter[X] = impl { fn get() X }
fn f[T](T x) i32 {
    match T {
        Getter[i32] { return 1 }
        else { return 0 }
    }
}
fn main() i32 { R r = { .v = 1 }  return f(r) }
