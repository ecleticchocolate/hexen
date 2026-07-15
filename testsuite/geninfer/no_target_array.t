//@ expect err cannot infer
fn id[T](T v) T { return v }
fn sink[T](T v) i32 { return 0 }
fn main() i32 {
    return sink(id({1, 2, 3}))
}
