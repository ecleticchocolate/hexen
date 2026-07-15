//@ expect val 42
fn add_one(u32 x) u32 { return x + 1 }
fn get_fn() fn(u32) u32 { return add_one }
fn main() i32 {
    fn(u32) u32 f = get_fn()
    return (i32) f(41)
}
