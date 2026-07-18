//@ expect val 15
enum Res[T] { T Ok i32 Err }
enum Node[T] { Res[T][2] Branch T Leaf }
fn sum_node[T](Node[T] n, fn(T) i32 f) i32 {
    match n {
        .Branch(b) {
            i32 acc = 0
            for u32 i = 0 to 2 {
                match b[i] {
                    .Ok(v) { acc = acc + f(v) }
                    .Err(e) { acc = acc + e }
                }
            }
            return acc
        }
        .Leaf(v) { return f(v) }
    }
    return 0
}
fn id(i32 x) i32 { return x }
fn main() i32 {
    Res[i32][2] arr = { .Ok(10), .Err(5) }
    Node[i32] n = .Branch(arr)
    return sum_node(n, id)
}
