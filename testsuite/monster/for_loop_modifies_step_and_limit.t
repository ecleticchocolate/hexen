//@ expect val 5
fn main() i32 {
    i32 iters = 0
    i32 limit = 5
    i32 step = 1
    for i32 i = 0 to limit by step {
        iters += 1
        limit = 10 
        step = 2   
    }
    return iters
}
