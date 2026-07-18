//@ expect val 50
enum Option[T] { T Some  None }
struct Node { i32 v  Node* next }
struct List { Node* head }
struct Cur { Node* p }
impl List { fn begin() Cur { return {.p = self.head} } }
impl Cur { fn next() Option[i32*] { if self.p == null { return .None } Node* n = self.p  self.p = n.next  return .Some(&n.v) } }
fn main() i32 {
    Node c = {.v = 30, .next = null}
    Node b = {.v = 20, .next = &c}
    Node a = {.v = 10, .next = &b}
    List l = {.head = &a}
    i32 s = 0
    for i32 x in l { if x == 20 { break } s = s + x }
    for i32 x in l { if x == 20 { continue } s = s + x }
    return s
}
