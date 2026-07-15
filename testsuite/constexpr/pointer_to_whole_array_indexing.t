//@ expect val 12
fn build() u32 {
    u32[8] arr
    u32 i = 0
    while i < 8 { arr[i] = i * 3  i = i + 1 }
    u32[8]* p = &arr
    return p[4]
}
const u32 ANSWER = build()
fn main() i32 { return (i32) ANSWER }
