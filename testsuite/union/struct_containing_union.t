//@ expect val 99
union Payload { i32 as_int  f32 as_float }
struct Msg { u32 tag  Payload data }
fn main() i32 {
    Msg m
    m.tag = 1
    m.data.as_int = 99
    return m.data.as_int
}
