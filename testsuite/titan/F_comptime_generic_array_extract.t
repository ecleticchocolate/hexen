//@ expect val 2
fn expand[T, u32 N](T[N] arr) T[N * 2] {
    T[N * 2] out
    u32 i = 0
    while i < N {
        out[i] = arr[i]
        out[i + N] = arr[i]
        i = i + 1
    }
    return out
}

fn main() i32 {
    i32[3] a = {1, 2, 3}
    i32[6] b = expand(a)
    return b[4]
}
