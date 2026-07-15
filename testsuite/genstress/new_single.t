//@ expect val 42
fn main() i32 {
    i32* p = new i32
    *p = 42
    i32 r = *p
    delete p
    return r
}
