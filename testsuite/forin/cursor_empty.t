//@ expect val 99
enum Option[T] { T Some  None }
struct Node { i32 v  Node* next }
struct List { Node* head }
struct Cur { Node* p }
impl List { fn begin() Cur { return {.p = self.head} } }
impl Cur { fn next() Option[i32*] { if self.p == null { return .None } Node* n = self.p  self.p = n.next  return .Some{&n.v} } }
fn main() i32 { List l = {.head = null}  i32 s = 99  for i32 x in l { s = s + x }  return s }
