//@ expect err no variant
enum Ev { u32 A  None }
fn main() i32 {
    Ev e = .Zzz(5)
    return 0
}
