//@ expect val 8
struct Holder { fn(u32) u32[4] slots }
fn main() i32 {
    // One fn-pointer field (8 bytes), NOT a 4-wide array of fn-pointers
    // (which would be 32). Confirms [4] bound to the return type u32, not
    // to the whole fn(u32)->u32 type.
    return (i32) sizeof(Holder)
}
