//@ expect val 50
enum Option[T]{ T Some  i64 None }
fn safe_div(i32 a,i32 b)Option[i32]{
    if b==0{return .None(0)}
    return .Some(a/b)
}
fn main()i32{
    Option[i32] r1=safe_div(10,2)
    Option[i32] r2=safe_div(10,0)
    i32 v1=99 i32 v2=99
    match r1{.Some(v) {v1=v} .None(n) {v1=0}}
    match r2{.Some(v) {v2=v} .None(n) {v2=0}}
    return v1*10+v2
}
