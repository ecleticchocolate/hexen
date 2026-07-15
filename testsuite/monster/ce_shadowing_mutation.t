//@ expect val 10
fn shadow_test() u32 {
    u32 x = 10
    if true {
        u32 x = 20
        x += 5
    }
    return x
}
const u32 X = shadow_test()
fn main() i32 { return (i32) X }
