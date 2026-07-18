//@ expect val 99
enum Option[T]{ T Some  i64 None }
fn main()i32{
    Option[i32] a=.None(0)
    match a{.Some(v) {return -1} .None(n) {return 99}}
    return 0
}
