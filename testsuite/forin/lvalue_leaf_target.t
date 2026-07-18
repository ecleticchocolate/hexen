//@ expect val 7
enum Option[T] { T Some  None }
struct Node { i32 x  i32 y  Node* next }
struct List { Node* head }
struct Cur { Node* p }
impl List { fn begin() Cur { return {.p = self.head} } }
impl Cur { fn next() Option[Node*] { if self.p == null { return .None } Node* n = self.p  self.p = n.next  return .Some(n) } }
fn main() i32 {
    Node b = {.x=3,.y=4,.next=null}
    Node a = {.x=1,.y=2,.next=&b}
    List l = {.head=&a}
    i32[2] outs={0,0}
    u32 k=0
    for unpack { .x=outs[0], .y=outs[1] } in l { k=k }
    return outs[0]+outs[1]
}
