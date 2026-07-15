//@ expect val 25
fn f[T](T x) i32 {
    match T {
        E[N] { u32[N] scratch  u32 i=0  while i<N { scratch[i]=i*i  i=i+1 }  return (i32)scratch[N-1] }
        else { return -1 }
    }
}
fn main() i32 { u8[6] a  return f(a) }
