//@ expect val 60
fn width(u32 n) u32 { return n + 0 }
struct Action[T, u32 N] { (fn(T[width(N)]) T) execute }
impl Action[T, N] { fn run(T[width(N)]* data) T { return self.execute(*data) } }
fn sum_3(i32[3] arr) i32 { return arr[0] + arr[1] + arr[2] }
fn main() i32 {
    Action[i32, 3] a
    a.execute = sum_3
    i32[width(3)] data
    data[0] = 10; data[1] = 20; data[2] = 30
    return a.run(&data)
}
