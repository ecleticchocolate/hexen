//@ expect val 42
enum Option[T]{ T Some  i64 None }
fn main()i32{
    Option[i32] a=.Some{42}
    match a{.Some{v} {return v} .None{n} {return 0}}
    return 0
}
