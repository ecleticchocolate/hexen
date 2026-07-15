//@ expect err type mismatch in function argument: cannot assign u32[8] to u32[4]
fn copy_arr[T](T[4] src) T[4] { T[4] dst; return dst }
fn main() i32 {
    u32[8] wrong_size = {1,2,3,4,5,6,7,8}
    u32[4] b = copy_arr(wrong_size)
    return 0
}
