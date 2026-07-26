//@ use std/option.t std/iterator.t std/vector.t std/array.t std/hashmap.t std/linkedlist.t
//@ expect stdout
//@ | vec f64: len=3 idx=2 eq=1
//@ | vec struct: x=3 y=4 eq=1
//@ | arr f64: idx=2 eq=1
//@ | arr struct: x=7 eq=1
//@ | list f64: len=3 idx=2 eq=1
//@ | list struct: x=3 eq=1
//@ | map f64 value: v=2
//@ | map struct value: x=3
//@ | map struct key: found=9
extern fn printf(u8* fmt, ...) i32
struct P { i32 x  i32 y }
fn main() i32 {
    // --- Vector ---
    Vector[f64] vf = Vector[f64].create()
    vf.push(1.5)  vf.push(2.5)  vf.push(3.5)
    Vector[f64] vf2 = vf.copy()
    printf("vec f64: len=%d idx=%d eq=%d\n", (i32)vf.len(), (i32)vf[1], (i32)(vf == vf2))
    Vector[P] vp = Vector[P].create()
    P p = {.x = 3, .y = 4}
    vp.push(p)
    Vector[P] vp2 = vp.copy()
    printf("vec struct: x=%d y=%d eq=%d\n", vp[0].x, vp[0].y, (i32)(vp == vp2))
    // --- Array ---
    Array[f64, 3] af
    af.fill(2.5)
    Array[f64, 3] af2 = af.copy()
    printf("arr f64: idx=%d eq=%d\n", (i32)af[1], (i32)(af == af2))
    Array[P, 2] ap
    P q = {.x = 7, .y = 8}
    ap.fill(q)
    Array[P, 2] ap2 = ap.copy()
    printf("arr struct: x=%d eq=%d\n", ap[0].x, (i32)(ap == ap2))
    // --- LinkedList ---
    LinkedList[f64] lf = LinkedList[f64].create()
    lf.push_back(1.5)  lf.push_back(2.5)  lf.push_back(3.5)
    LinkedList[f64] lf2 = lf.copy()
    printf("list f64: len=%d idx=%d eq=%d\n", (i32)lf.len(), (i32)lf[1], (i32)(lf == lf2))
    LinkedList[P] lp = LinkedList[P].create()
    lp.push_back(p)
    LinkedList[P] lp2 = lp.copy()
    printf("list struct: x=%d eq=%d\n", lp[0].x, (i32)(lp == lp2))
    // --- HashMap ---
    HashMap[i32, f64] mf = HashMap[i32, f64].create()
    mf[1] = 2.5
    printf("map f64 value: v=%d\n", (i32)mf[1])
    HashMap[i32, P] mp = HashMap[i32, P].create()
    mp[1] = p
    printf("map struct value: x=%d\n", mp[1].x)
    HashMap[P, i32] mk = HashMap[P, i32].create()
    mk[p] = 9
    P same = {.x = 3, .y = 4}
    printf("map struct key: found=%d\n", mk[same])
    return 0
}
