//@ expect val 3
fn isalpha(u8 c)bool{return (c>=65&&c<=90)||(c>=97&&c<=122)} fn main()i32{return (i32)isalpha(65)+(i32)isalpha(122)*2+(i32)isalpha(48)*4}
