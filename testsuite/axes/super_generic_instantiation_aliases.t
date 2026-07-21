//@ expect val 12
// `super BaseG[T] base` -- a super whose embedded type is a generic
// INSTANTIATION mentioning the enclosing template's own parameter.
//
// This takes NEITHER of the two paths that already worked:
//   * `super T base`        -> is_super_param, spliced in Struct_Instantiate
//   * `super BaseG[u8] base` -> concrete, spliced at parse time
// It is spliced at PARSE time (its type is a TYPE_STRUCT, not a TYPE_PARAM,
// so is_super_param never fires), which puts the promoted prefix and the
// packaged alias on the TEMPLATE -- and then Struct_Instantiate's general
// field-copy path rebuilt each field without is_super_alias /
// super_prefix_span. Struct_Layout consequently gave the package its own
// storage instead of pointing it at the prefix: sizeof was 20 (12 + a whole
// duplicated BaseG[u8]) and writes through d.id vs d.info.id landed at
// different offsets with no diagnostic.
//
// 12 = BaseG[u8]{u8 tag, u32 id} (8, padded) + u32 extra (4), counted once.
struct BaseG[T] { T tag  u32 id }
struct DerG[T]  { super BaseG[T] info  u32 extra }
fn main() i32 {
    DerG[u8] d
    d.id = 7
    d.info.id = 555        // same bytes as d.id -- must be visible through it
    if d.id != 555 { return -1 }
    d.id = 3
    if d.info.id != 3 { return -2 }
    return (i32)sizeof(DerG[u8])
}
