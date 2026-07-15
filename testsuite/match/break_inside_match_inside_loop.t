//@ expect val 10
enum Cmd { u32 Stop  u32 Go }
fn make_cmd(i32 i) Cmd {
    if i == 5 { return .Stop{(u32)i} }
    return .Go{(u32)i}
}
fn main() i32 {
    i32 acc = 0
    for i32 i = 0 to 10 {
        match make_cmd(i) {
            .Stop{v} { break }
            .Go{v} { acc += (i32) v }
        }
    }
    return acc
}
