//@ expect val 10
// Cursor-protocol edge: Payload[i32] is instantiated and used BEFORE the
// for-in loop ever runs, so by the time the cursor's next() resolves
// Option[Payload[T]]'s return type through specialize_return, Payload[i32]
// is already a cached StructDef (via an earlier, unrelated instantiation
// site) -- stresses that reusing the cached instantiation doesn't diverge
// from a fresh one built by the cursor's own substitution.
enum Option[T] { T Some  None }
struct Payload[T] { T val }
struct Box[T] { Payload[T][2] items  u32 pos }
struct Cur[T] { Payload[T][2] items  u32 pos }
impl Box[T] { fn begin() Cur[T] { return {.items = self.items, .pos = 0} } }
impl Cur[T] {
    fn next() Option[Payload[T]] {
        if self.pos >= 2 { return .None }
        Payload[T] p = self.items[self.pos]
        self.pos = self.pos + 1
        return .Some(p)
    }
}
fn main() i32 {
    // Force Payload[i32] to already exist in the struct registry before
    // the for-in loop below ever calls begin()/next().
    Payload[i32] pre = {.val = 999}
    i32 keep_alive = pre.val

    Box[i32] b = {.items = { {.val=3}, {.val=7} }, .pos = 0}
    i32 sum = 0
    for Payload[i32] p in b {
        sum = sum + p.val
    }
    return sum
}
