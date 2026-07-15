//@ expect val 5
fn main() i32 {
    i32 acc = 0
    for i32 i = 0 to 6 {
        if i == 3 { continue }
        acc += 1
    }
    return acc
}
