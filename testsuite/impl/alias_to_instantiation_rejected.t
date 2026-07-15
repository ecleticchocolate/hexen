//@ expect err specialization
// The same specialization request, arriving under an alias. The call site cannot reach
// it either (it mangles `Box_tag`, never `Box[u8]_tag`), so left alone this would
// register a symbol nothing can ever call -- the method would simply not exist, with no
// diagnostic.
struct Box[T] { T v }
alias B8 = Box[u8]
impl B8 { fn tag() u32 { return 8 } }
fn main() u32 { return 0 }
