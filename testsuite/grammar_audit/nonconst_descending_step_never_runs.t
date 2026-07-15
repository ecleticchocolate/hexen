//@ expect val 10
i32 g_step = 1
fn get_step() i32 { return g_step }
fn main() i32 {
    g_step = 0 - 1
    i32 iters = 0
    for i32 i = 10 to 0 by get_step() {
        iters = iters + 1
        if iters > 20 { return -999 }
    }
    return iters
}
