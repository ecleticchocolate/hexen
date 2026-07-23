//@ expect err undeclared identifier x

fn main() i32 {
    // x is not declared yet when typeof(x) is evaluated -> REJECTED!
    typeof(x) x = 10
    return x
}
