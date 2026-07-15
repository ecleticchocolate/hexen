//@ expect val 9
fn main() i32 {
    i32 acc = 0
    for i32 i = 0 to 3 {
        for i32 j = 0 to 3 {
            acc += 1
        }
    }
    return acc
}
