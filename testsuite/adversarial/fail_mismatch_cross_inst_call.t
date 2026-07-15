//@ expect val 0
struct Box[T] { T val }
fn make_box_arr[T](T a, T b) Box[T][2] { Box[T][2] arr; return arr }
fn main() i32 {
    Box[i32][2] ib = make_box_arr(3, 4)
    return 0
}
