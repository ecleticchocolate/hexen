//@ expect val 300
with pub const u32 {
    X = 100
    Y = 200
}
fn main() i32 { return (i32)(X + Y) }
