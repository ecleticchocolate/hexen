//@ expect val 123456789
fn sum10(i32 a,i32 b,i32 c,i32 d,i32 e,i32 f,i32 g,i32 h,i32 i,i32 j) i32 {
    return a*1 + b*10 + c*100 + d*1000 + e*10000 + f*100000 + g*1000000 + h*10000000 + i*100000000 + j
}
fn main() i32 { return sum10(9,8,7,6,5,4,3,2,1,0) }
