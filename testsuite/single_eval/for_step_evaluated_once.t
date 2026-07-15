//@ expect val 5001
i32 step_calls = 0
fn get_step() i32 {
    step_calls = step_calls + 1
    return 1
}
fn main() i32 {
    i32 iters = 0
    for i32 i = 0 to 5 by get_step() {
        iters = iters + 1
    }
    return iters * 1000 + step_calls
}
