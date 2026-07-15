//@ expect val 42
fn isdig(u8 c)bool{return c>=48&&c<=57} fn main()i32{ u8* s="42abc" u32 n=0 i64 v=0 while isdig(s[n]){v=v*10+(i64)(s[n]-48) n=n+1} return (i32)v }
