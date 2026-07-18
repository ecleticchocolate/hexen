//@ expect val 1
struct Box[T, u32 N] { T[N] items }
enum Wrapper { Box[u32, 3] Nums  None }
fn main() i32 {
    Wrapper a = .Nums( { .items = {1, 2, 3} } )
    Wrapper b = .Nums( { .items = {1, 2, 3} } )
    return (i32)(a == b)
}
