//@ expect val 1
// Found while stress-testing the enum pack-tail fix: inside a GENERIC function,
// a CONCRETE (non-wildcard) type in an enum pattern's field position -- `u32 h`,
// not `H h` -- never matched anything, even against a real enum whose first
// variant genuinely IS u32. Root cause: a THIRD site with the same "loses
// is_enum on rebundle" bug already fixed twice elsewhere (Struct_MakeAnon,
// Struct_Instantiate) -- Type_Substitute's own anonymous-struct re-registration
// path (used when a generic function's body re-resolves a pattern type during
// instantiation) also hardcoded "struct{" naming and is_enum=false. A WILDCARD
// pattern (`H h`) happened to take a different path and worked, masking this.
enum Shape3 { u32 Circle }

fn probe[T]() i32 {
    match T {
        enum { u32 } { return 1 }
        else { return 999 }
    }
}

fn main() i32 { return probe[Shape3]() }
