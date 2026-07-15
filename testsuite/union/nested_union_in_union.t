//@ expect val 8
union Inner { i32 a  f32 b }
union Outer { Inner inner  i64 raw }
fn main() i32 {
    Outer o
    o.raw = 4607182418800017408
    return (i32) sizeof(Outer)
}
