//@ expect val 3001
i32 limit_calls = 0
fn limit() i32 {
    limit_calls = limit_calls + 1
    return 3
}
fn main() i32 {
    i32 iters = 0
    for i32 i = 0 to limit() {
        iters = iters + 1
    }
    return iters * 1000 + limit_calls
}
