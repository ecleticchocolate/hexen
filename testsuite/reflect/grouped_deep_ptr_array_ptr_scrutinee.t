//@ expect val 202
struct Box[T]{ T v }
fn main() i32 {
    match (fn() Box[u16])*[2]* {
        A*[N]* {
            match A { fn() Box[E] { return (i32)N*100 + (i32)sizeof(E) } else { return -1 } }
        }
        else { return -2 }
    }
}
