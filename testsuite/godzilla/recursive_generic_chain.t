//@ expect val 15
enum Opt[T] { T Some  None }
struct Node[T] { T val  Opt[u32] next_idx }
fn sum_chain(Node[u32][4] arr, u32 idx) u32 {
    Node[u32] n = arr[idx]
    match n.next_idx {
        .Some(next) { return n.val + sum_chain(arr, next) }
        .None { return n.val }
    }
    return 0
}
fn main() i32 {
    Node[u32][4] chain = {
        {.val = 1, .next_idx = .Some(1)},
        {.val = 2, .next_idx = .Some(2)},
        {.val = 4, .next_idx = .Some(3)},
        {.val = 8, .next_idx = .None}
    }
    return (i32) sum_chain(chain, 0)
}
