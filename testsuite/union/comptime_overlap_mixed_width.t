//@ expect val 68
// Comptime write through a wide field, read back through a narrow field at
// the same offset. Only passes if ConstEval's aggregate storage is truly
// byte-addressed (matches Struct_Layout's overlap offsets) rather than
// treating each union field as independent storage. 0x11223344 -> low byte
// 0x44 = 68 (little-endian), same as the runtime JIT path.
union U { u8 small  i32 big }
fn make() i32 {
    U u
    u.big = 0x11223344
    return (i32) u.small
}
const i32 R = make()
fn main() i32 { return R }
