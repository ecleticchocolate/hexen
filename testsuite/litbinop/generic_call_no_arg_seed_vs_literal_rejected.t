//@ expect err cannot infer
fn make_default[T]() T { T v; return v }
fn main() u32 {
    if make_default() == {1, 2, 3, 4} { return 1 }
    return 0
}
