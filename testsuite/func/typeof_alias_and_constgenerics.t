//@ expect val 109
struct Box[T, u32 N] {
    T[N] data
}

fn get_box_size[T, u32 N](Box[T, N]* b) i32 {
    return (i32)N
}

fn main() i32 {
    // 1. Alias with typeof(expr):
    i32 sample_val = 42
    alias MyType = typeof(sample_val)
    MyType val = 100

    // 2. Generic type arguments with typeof(expr):
    Box[typeof(sample_val), 5] b
    i32 size = get_box_size[typeof(sample_val), 5](&b) // 5

    // 3. sizeof(typeof(expr)) inside const generic size argument:
    Box[i32, sizeof(typeof(sample_val))] b2 // sizeof(i32) = 4
    i32 size2 = get_box_size[i32, sizeof(typeof(sample_val))](&b2) // 4

    return val + size + size2 // 100 + 5 + 4 = 109
}
