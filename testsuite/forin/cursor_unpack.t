//@ expect val 10
enum Option[T] { T Some  None }
struct P { i32 x  i32 y }
struct Node { P v  Node* next }
struct List { Node* head }
struct Cur { Node* p }
impl List { fn begin() Cur { return {.p = self.head} } }
impl Cur { fn next() Option[P*] { if self.p == null { return .None } Node* n = self.p  self.p = n.next  return .Some(&n.v) } }
fn main() i32 {
    Node b = {.v = {.x=3,.y=4}, .next=null}
    Node a = {.v = {.x=1,.y=2}, .next=&b}
    List l = {.head=&a}
    i32 s = 0
    for unpack {.x = px, .y = py} in l { s = s + px + py }
    return s
}
