//@ expect stdout
//@ | 42
//@ | sizeof(U)=4
// Found while stress-testing the enum pack-tail fix (same root cause, different
// axis): Struct_MakeAnon -- the helper that rebundles a Rest... tail into a
// fresh anonymous type -- unconditionally set is_enum=false AND never set
// is_overlapping, so a UNION's Rest tail silently became a plain STRUCT: fields
// stacked one after another instead of overlapping at offset 0. A real,
// previously-untested bug, not introduced by the enum fix -- confirmed absent
// from the existing suite before this test was added.
extern fn printf(u8* fmt, ...) i32
union U { i32 a  i32 b  i32 c }

fn read_offset0[Walk](Walk* p) i32 {
    match Walk {
        union { H; Rest... } { return *(i32*)p }
        else { return -1 }
    }
}
fn peel_one[Walk](Walk* p) i32 {
    match Walk {
        union { H; Rest... } { return read_offset0[Rest]((Rest*)p) }
        else { return -1 }
    }
}

fn main() i32 {
    U u = { .a = 42 }
    // If Rest correctly stayed a UNION (overlapping), field 0 of the peeled
    // Rest type, read at the SAME address, must still read 42 (offset 0). If
    // it silently became a STRUCT, "field 0" would sit at a different offset.
    printf("%d\n", peel_one(&u))
    printf("sizeof(U)=%d\n", (i32)sizeof(u))
    return 0
}
