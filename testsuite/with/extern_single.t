//@ expect val 7
with extern {
    fn printf(u8* fmt, ...) i32
}
fn main() i32 { printf("hi\n")  return 7 }
