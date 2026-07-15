//@ expect val 3
// specs.md §9: the bound and step are each evaluated once, at loop entry.
// Mutating the SOURCE variables inside the body must not change the loop's
// shape mid-flight -- bound/step are hoisted into hidden locals at entry, so
// these writes only affect `lim`/`stp` themselves, not the running loop.
i32 lim = 3
i32 stp = 1
fn main() i32 {
    i32 iters = 0
    for i32 i = 0 to lim by stp {
        iters = iters + 1
        lim = 100
        stp = 10
    }
    return iters
}
