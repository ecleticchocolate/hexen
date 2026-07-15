//@ expect val 0
fn one_field[T](T x) u32 {
    match T {
        struct { A a  Rest... r } { return (u32)sizeof(Rest) }
        else { return 99 }
    }
}
fn main() i32 {
    struct { i32 p } v
    return (i32)one_field(v)
}
