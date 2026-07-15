//@ expect val 1
i32 call_count = 0
fn n() i32 {
    call_count = call_count + 1
    return 5
}
fn main() i32 {
    u8* buf = new[n()] u8
    return call_count
}
