//@ expect val 9
enum Parity { u32 Even  u32 Odd }
fn parity(u32 i) Parity {
    if i % 2 == 0 { return .Even(i) }
    return .Odd(i)
}
fn main() i32 {
    i32 acc = 0
    for u32 i = 0 to 6 {
        match parity(i) {
            .Even(v) { continue }
            .Odd(v) { acc += (i32) v }
        }
    }
    return acc
}
