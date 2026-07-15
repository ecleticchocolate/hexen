//@ expect val 5
fn main() i32 {
    i32 acc = 0
    for i32 i = 0 to 100 {
        if i == 5 { break }
        acc += 1
    }
    return acc
}
