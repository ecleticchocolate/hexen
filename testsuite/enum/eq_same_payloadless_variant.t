//@ expect val 1
enum Dir { North  South  East  West }
fn main() i32 {
    Dir a = .North
    Dir b = .North
    return (i32)(a == b)
}
