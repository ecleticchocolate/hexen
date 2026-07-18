//@ expect val 200
enum Option[T] { T Some  None }
fn get_second[T](T* arr) Option[T] {
    return .Some(arr[1])
}
fn main() i32 {
    u32* heap = new[3] u32
    heap[0] = 100; heap[1] = 200; heap[2] = 300
    u32** pp = &heap
    Option[u32] r = get_second(*pp)
    delete heap
    match r {
        .Some(v) { return (i32) v }
        .None { return -1 }
    }
    return -2
}
