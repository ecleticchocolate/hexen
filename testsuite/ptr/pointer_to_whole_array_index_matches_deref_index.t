//@ expect val 99
fn main() i32 {
    u32[8] arr
    arr[3] = 99
    u32[8]* p = &arr
    // p[i] must auto-deref then index (specs.md SS8: "array indexing auto-derefs
    // through a pointer") -- p[3] on a T[8]* must equal arr[3] directly (a
    // scalar u32), not the whole T[8] array. A prior bug had p[i]s TYPE come
    // out as T[8] itself (the pointee type, un-auto-derefed), so this used to
    // fail to typecheck as a scalar at all.
    return (i32) p[3]
}
