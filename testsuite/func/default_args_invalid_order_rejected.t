//@ expect err non-default parameter
fn f(i32 x = 10, i32 y) i32 {
    return x + y
}

fn main() i32 {
    return f(1, 2)
}
