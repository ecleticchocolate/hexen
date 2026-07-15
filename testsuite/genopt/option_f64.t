//@ expect val 314
enum Option[T]{ T Some  i64 None }
fn main()i32{
    Option[f64] a=.Some{3.14}
    match a{.Some{v} {return (i32)(v*100.0)} .None{n} {return 0}}
    return 0
}
