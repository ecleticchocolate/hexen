//@ expect val 3
enum A { u32 X  u32 Y }
enum B { u32 P  u32 Q }
fn main() i32 {
    A a = .X(1)
    B b = .Q(2)
    match a {
        .X(v) {
            match b {
                .P(w) { return (i32) v + (i32) w + 10 }
                .Q(w) { return (i32) v + (i32) w }
            }
        }
        .Y(v) { return -1 }
    }
    return -2
}
