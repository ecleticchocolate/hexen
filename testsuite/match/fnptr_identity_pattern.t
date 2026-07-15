//@ expect val 42
fn double(u32 x) u32 { return x * 2; }
fn main() i32 {
    fn(u32) u32 fp = double;
    match fp {
        g { return (i32) g(21); }
        else { return -1; }
    }
}
