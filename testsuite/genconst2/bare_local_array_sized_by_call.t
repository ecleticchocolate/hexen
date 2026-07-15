//@ expect val 842
fn width(u32 n) u32 { return n + 5 }
fn main() i32 {
    i32[width(3)] data
    data[7] = 42
    return (i32)(sizeof(data)/sizeof(i32)) * 100 + data[7]
}
