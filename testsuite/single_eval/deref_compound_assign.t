//@ expect val 1015
i32 storage = 10
i32 call_count = 0
fn ptr() i32* {
    call_count = call_count + 1
    return &storage
}
fn main() i32 {
    *ptr() += 5
    return storage + call_count * 1000
}
