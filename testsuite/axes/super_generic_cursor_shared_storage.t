//@ expect val 6
// Axis cross: super-embedding a generic struct that itself participates in
// the for-in cursor protocol (BaseCur[T] both drives a for-in loop via
// Holder[T].begin(), AND is super-embedded in Box[T]). Confirms the
// super-aliasing rule (REFERENCE.md `super`: the packaged field IS the
// promoted prefix, same storage, not a second copy) still holds when the
// embedded type is a generic INSTANTIATION.
//
// Regression: Struct_Instantiate's general field-copy path dropped
// is_super_alias/super_prefix_span, so `super BaseCur[T] base` -- already
// spliced at parse time, hence never taking the is_super_param path --
// lost its aliasing on every instantiation. The prefix and the package were
// then laid out SEQUENTIALLY: sizeof over-reported by sizeof(Base), and
// b.pos and b.base.pos silently became different memory. The concrete-base
// case was unaffected, so the two disagreed. b.pos - b.base.pos is 0 here
// precisely because they are the same bytes.
enum Option[T] { T Some  None }
struct BaseCur[T] { T[3] items  u32 pos }
impl BaseCur[T] {
    fn next() Option[T] {
        if self.pos >= 3 { return .None }
        T v = self.items[self.pos]
        self.pos = self.pos + 1
        return .Some(v)
    }
}
struct Box[T] { super BaseCur[T] base  u32 extra }
struct Holder[T] { T[3] items  u32 pos }
impl Holder[T] {
    fn begin() BaseCur[T] { return {.items = self.items, .pos = 0} }
}
fn main() i32 {
    Holder[i32] h = {.items = {1,2,3}, .pos = 0}
    i32 sum = 0
    for i32 x in h { sum = sum + x }
    Box[i32] b = {.base = {.items = {0,0,0}, .pos = 0}, .extra = 9}
    // touch the super-embedded field through both promoted access and the
    // explicit .base: same storage, so the difference is 0
    b.pos = 1
    return sum + (i32)(b.pos - b.base.pos)
}
