//@ expect val 28
struct Tree { u32 val  Tree* left  Tree* right }
fn leaf(u32 v) Tree* { return new Tree{.val = v, .left = null, .right = null} }
fn node(u32 v, Tree* l, Tree* r) Tree* { return new Tree{.val = v, .left = l, .right = r} }
fn sum_tree(Tree* t) u32 {
    if t == null { return 0 }
    return t.val + sum_tree(t.left) + sum_tree(t.right)
}
fn build() u32 {
    //        4
    //       / \        recursively allocated on the comptime heap,
    //      2   6        then recursively summed by chasing pointers
    //     /\   /\
    //    1 3  5 7
    Tree* root = node(4, node(2, leaf(1), leaf(3)), node(6, leaf(5), leaf(7)))
    return sum_tree(root)
}
const u32 TOTAL = build()   // 1+2+3+4+5+6+7 = 28, at compile time
fn main() i32 { return (i32)TOTAL }
