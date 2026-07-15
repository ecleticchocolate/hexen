//@ expect val 10
fn main() i32 {
    i32 acc = 0
    i32 i = 0
    while true {
        if i >= 5 { break }
        acc += i
        i += 1
    }
    return acc
}
