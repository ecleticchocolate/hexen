//@ expect val 6
enum Option[T] { T Some  None }
struct Node { i32 v  Node* next }
struct List { Node* head }
struct Cur { Node* p }
impl List { fn begin() Cur { return {.p = self.head} } }
impl Cur { fn next() Option[i32*] { if self.p == null { return .None } Node* n = self.p  self.p = n.next  return .Some{&n.v} } }
fn go(List l) i32 { i32 s = 0  for i32 x in l { s = s + x }  return s }
fn main() i32 { Node c = {.v=3,.next=null}  Node b = {.v=2,.next=&c}  Node a = {.v=1,.next=&b}  List l = {.head=&a}  return go(l) }
