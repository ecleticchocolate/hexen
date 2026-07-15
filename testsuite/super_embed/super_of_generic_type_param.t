//@ expect val 4017
// `super T base` inside a generic template: T is an unresolved TYPE_PARAM at
// template-parse time, so there's nothing to promote fields from yet. The
// promotion is deferred to Struct_Instantiate, which expands the single
// template field into N instance fields once T is bound to a concrete
// struct. Two different instantiations (Holder[A], Holder[C]) must each get
// their own correctly-expanded, non-crosstalking field set.
struct A { u32 x }
struct C { u32 w  u32 z }
struct Holder[T] {
    super T base
    u32 tag
}
fn main() i32 {
    Holder[A] h1
    h1.x = 3
    h1.tag = 9
    h1.base.x = 100

    Holder[C] h2
    h2.w = 1
    h2.z = 2
    h2.tag = 5

    return h1.x*1000 + h1.tag*100 + h1.base.x + h2.w*10 + h2.z + h2.tag
}
