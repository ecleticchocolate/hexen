//@ expect val 10
struct Stack { i64[32] d  u32 sp }
impl Stack {
    fn push(i64 v) { self.d[self.sp] = v  self.sp = self.sp + 1 }
    fn pop() i64 { self.sp = self.sp - 1  return self.d[self.sp] }
}
fn eval_rpn(u8* s, u32 len) i64 {
    Stack st = {.sp = 0}
    u32 i = 0
    while i < len {
        u8 c = s[i]
        if c >= 48 { if c <= 57 { st.push((i64)(c - 48)) } }
        if c == 43 { i64 b = st.pop()  i64 a = st.pop()  st.push(a + b) }
        if c == 45 { i64 b = st.pop()  i64 a = st.pop()  st.push(a - b) }
        if c == 42 { i64 b = st.pop()  i64 a = st.pop()  st.push(a * b) }
        i = i + 1
    }
    return st.pop()
}
fn go() i64 {
    u8[13] prog = {51,32,52,32,50,32,42,32,43,32,49,32,45}
    return eval_rpn(&prog[0], 13)
}
const i64 ANSWER = go()
fn main() i32 {
    return (i32) ANSWER
}
