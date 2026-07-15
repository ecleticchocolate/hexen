//@ expect val 5
fn main() i32 {
    i32 v = 5;
    i32* p = &v;
    unpack q = p;
    return *q;
}
