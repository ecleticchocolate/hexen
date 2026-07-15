//@ expect err operator not defined on
enum Dir { North  South  East  West }
fn main() i32 {
    Dir a = .North
    Dir b = .South
    Dir c = a + b
    return 0
}
