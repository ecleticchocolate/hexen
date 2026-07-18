//@ expect val 88
struct V { i32 x }
fn main() i32 {
    i32[3] a = {1, 2, 3}
    i32* p = &a[0]
    V val = {.x = 18}
    *(p + (val.x/9)) = 88
    return a[2]
}
