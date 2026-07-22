//@ expect val 100
// `*name` gives MATCH write-through binding with no `match&` and no references --
// the pointer is the alias, visible in the pattern. Read/write goes through *pv.
struct Box { i32 v }
fn main() i32 {
    Box b = {.v=1}
    match b {
        {.v=*pv} { *pv = 100  return *pv }
        else {}
    }
    return -1
}
