//@ expect val 8021412
fn andarr(u32[2] a, u32[2] b) u32[2] { return a & b }
fn orarr(u32[2] a, u32[2] b) u32[2] { return a | b }
fn xorarr(u32[2] a, u32[2] b) u32[2] { return a ^ b }
const u32[2] r1 = andarr({12, 10}, {10, 6})
const u32[2] r2 = orarr({12, 10}, {10, 6})
const u32[2] r3 = xorarr({12, 10}, {10, 6})
fn main() u32 { return r1[0]*1000000 + r1[1]*10000 + r2[0]*100 + r3[1] }
