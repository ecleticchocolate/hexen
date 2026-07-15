//@ expect val 42
enum Result[T] { T Ok  i32 Err }
fn safe_div[T](T a, T b) Result[T] {
    if b == 0 { return .Err{-1} }
    return .Ok{a / b}
}
fn main() i32 {
    fn(i32, i32) Result[i32] f = safe_div
    Result[i32] r = f(84, 2)
    match r { .Ok{v} { return v }  .Err{e} { return e } }
    return 0
}
