//@ expect val 4
fn pack_len[T](T dummy) u32 {
    match T {
        struct {} { return 0 }
        struct { A a  Rest... r } {
            Rest r2 = {}
            return 1 + pack_len(r2)
        }
        else { return 999 }
    }
}
fn main() i32 {
    struct { i32 a  i32 b  i32 c  i32 d } v = {}
    return (i32)pack_len(v)
}
