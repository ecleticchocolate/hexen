//@ expect err cannot infer
enum Option[T] { T Some  None }
fn id[T](T v) T { return v }
fn sink[T](T v) i32 { return 0 }
fn main() i32 {
    return sink(id(.Some{5}))
}
