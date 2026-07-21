//@ expect val 0
fn pack_len[T](T dummy) u32 {
    match T {
        struct {  } { return 0 }
        struct { A; Rest... } {
            Rest r2 = {}
            return 1 + pack_len(r2)
        }
        else { return 999 }
    }
}
fn main() i32 {
    struct {} v = {}
    return (i32)pack_len(v)
}
