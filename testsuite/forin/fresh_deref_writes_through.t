//@ expect val 100
// for-unpack with a fresh `*px` leaf: px aliases the live element, `*px = v`
// mutates the source node in place (no copy) -- Hexen's `auto&`.
enum Option[T] { T Some  None }
struct Node { i32 x  i32 y  Node* next }
struct List { Node* head }
struct Cur { Node* p }
impl List { fn begin() Cur { return {.p = self.head} } }
impl Cur { fn next() Option[Node*] { if self.p == null { return .None } Node* n = self.p  self.p = n.next  return .Some(n) } }
fn main() i32 {
    Node a = {.x=1,.y=2,.next=null}
    List l = {.head=&a}
    for unpack {.x=*px, .y=*py} in l { *px = 100 }
    return a.x
}
