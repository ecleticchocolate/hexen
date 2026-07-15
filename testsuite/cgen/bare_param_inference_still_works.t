//@ expect val 5
fn first[T, N](T[N] arr) T { return arr[0] }
fn main() i32 {
    i32[4] a = {5, 6, 7, 8}
    return first(a)
}
