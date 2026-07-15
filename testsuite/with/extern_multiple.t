//@ expect val 42
with extern {
    fn printf(u8* fmt, ...) i32
    fn malloc(u64 n) u8*
    fn free(u8* p)
}
fn main() i32 { printf("ok\n")  return 42 }
