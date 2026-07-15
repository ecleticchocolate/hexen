//@ expect val 60
struct Box[T] { T val }
fn compute() f64 {
    Box[f32][3] arr = { {.val=1.5}, {.val=2.5}, {.val=3.5} }
    f64 sum = 0.0
    for u32 i = 0 to 3 {
        if arr[i].val > 2.0 {
            sum = sum + (f64)arr[i].val
        }
    }
    return sum
}
const f64 R = compute()
fn main() i32 { return (i32)(R * 10.0) }
