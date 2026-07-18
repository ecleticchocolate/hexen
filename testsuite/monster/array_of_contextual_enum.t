//@ expect val 10
enum Opt { u32 S  None }
fn main() i32 {
    Opt[2] a = {.S(10), .None}
    match a[0] { .S(v) { return (i32) v }  .None { return -1 } }
    return -2
}
