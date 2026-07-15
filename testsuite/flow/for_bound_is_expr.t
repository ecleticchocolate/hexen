//@ expect val 10
fn len() u32 { return 5 }
fn main() i32 {
    i32 acc = 0
    for u32 i = 0 to len() {
        acc += (i32) i
    }
    return acc
}
