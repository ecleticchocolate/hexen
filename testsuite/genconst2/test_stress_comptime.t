//@ expect stdout
//@ | sizeof final array: 140
extern fn printf(u8* fmt, ...) i32

struct Tree { u32 val  Tree* left  Tree* right }

fn leaf(u32 v) Tree* { return new Tree{.val = v, .left = null, .right = null} }
fn node(u32 v, Tree* l, Tree* r) Tree* { return new Tree{.val = v, .left = l, .right = r} }

fn get_magic_value[T](u32 depth) u32 {
    u32 base_val = 0
    match depth {
        0 { base_val = 1 }
        1 { base_val = 2 }
        else { base_val = 3 }
    }
    
    match T {
        u32 { return base_val * 10 }
        E[N] { return base_val * N }
        else { return base_val }
    }
}

fn build_tree[T](u32 depth) Tree* {
    if depth == 0 { return null }
    u32 v = get_magic_value[T](depth)
    return node(v, build_tree[T](depth - 1), build_tree[T](depth - 1))
}

fn sum_tree(Tree* t) u32 {
    if t == null { return 0 }
    return t.val + sum_tree(t.left) + sum_tree(t.right)
}

struct Wrapper[T] {
    u32[sum_tree(build_tree[T](2))] data
}

fn main() i32 {
    Wrapper[u32[5]] w
    printf("sizeof final array: %d\n", sizeof(w))
    return 0
}
