//@ expect val 8
fn main() i32 {
    alias Pair[X] = struct { X a  X b }
    return (i32)sizeof(Pair[u32])
}
