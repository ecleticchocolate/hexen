//@ expect val 7
i32 g = 7
fn getg(i32 x) i32* { return &g }
fn main() i32 {
    fn(i32) i32* f = getg
    i32* p = f(0)
    return *p
}
