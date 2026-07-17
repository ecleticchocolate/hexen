//@ expect val 1
struct Box[T] { T v }
alias IntBox = Box[i32]
impl Box[T] {
    fn __eq(Box[T] other) bool { return self.v == other.v }
}
fn main() i32 {
    IntBox a = {.v = 5}
    IntBox b = {.v = 5}
    IntBox c = {.v = 9}
    bool r1 = (a == b)
    bool r2 = (a == c)
    match r1 {
        true {
            match r2 {
                false { return 1 }
                true { return 0 }
            }
        }
        false { return 0 }
    }
    return -1
}
