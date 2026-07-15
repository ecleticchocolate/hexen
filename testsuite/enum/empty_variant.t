//@ expect val 0
enum Signal { u32 Data  Empty }
fn main() i32 {
    Signal s = .Empty
    match s {
        .Data{v} { return (i32) v }
        .Empty { return 0 }
    }
    return -1
}
