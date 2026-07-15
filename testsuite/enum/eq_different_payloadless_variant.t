//@ expect val 0
enum Dir { North  South  East  West }
fn main() i32 {
    Dir a = .North
    Dir b = .South
    return (i32)(a == b)
}
