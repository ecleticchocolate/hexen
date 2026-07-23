""# Hexen Language Reference

Terse, complete, no rationale. One construct per entry: signature, then a
minimal example. For "why," see `specs.md`. For workflow, see `GUIDE.md`.

## Statement layout — READ FIRST

**One statement per line.** Do not put two statements on the same line unless
you genuinely have to. Cramming multiple statements onto one line is **not**
idiomatic Hexen and should not be treated as the house style — much of the
existing test corpus crams for brevity, but that is test shorthand, not a
model to imitate. Write normal, one-per-line code.

**Never put a second statement on a line when either statement dereferences.**
A leading `*` (or `&`) at a statement boundary is **ambiguous with a binary
operator against the previous expression**: `*px = 100  *py = 55` parses `100
*py` as a multiplication, not as two write-through assignments. This is not a
compiler bug — it is inherent to the grammar. Put each deref/write-through
statement on its own line:

```
*px = 100
*py = 55
```

```
extern fn printf(u8* fmt, ...) i32
fn main() i32 {
    printf("Hello, world!\n")
    return 0
}
```

---

## Lexical

```
// line comment
/* block comment */
0x1F  0b101  42  3.14  1.5e10
"string literal"        // u8* into static storage
'a'                     // u8
true  false  null
```

Statements end at newline. `;` is accepted, never required. Two statements
may share a line.

---

## Primitive types

```
u8 u16 u32 u64   i8 i16 i32 i64   f32 f64   bool   void
```

`void` = no return value in function-return position (written or omitted —
both mean the same thing). Elsewhere it's a genuine zero-sized unit type, not
specially restricted: `void x`, `void[N]`, a struct field, or a generic type
argument (`Box[void]`) are all legal and all cost 0 bytes; `void == void` is
always `true`. See Showcases for the idiomatic uses this enables.
`(void) expr` is an ordinary cast to this type — not a dedicated discard
syntax as in C, just the same cast grammar every other type already uses —
and reads naturally as "discard the result" since `void` holds nothing.

---

## Declarations

```
TYPE name              // variable
TYPE name = expr       // variable with initializer
TYPE name              // parameter, field — same grammar everywhere
```

No `let`, `var`, `:`. Type is always first.

```
fn name(TYPE a, TYPE b) RETTYPE { ... }
fn name(TYPE a)         { ... }          // omitted return = void
extern fn name(TYPE a) RETTYPE           // no body; links against libc/host
extern fn printf(u8* fmt, ...) i32       // "..." = C varargs (extern only)
pub fn name(...) ...                     // exported via -emit-mod
```

---

## Scoping

A bare `{ }` block is a real scope, identical in kind to an `if`/`while`/
`for` body:
```
i32 x = 1
{ i32 y = 2  x = x + y }   // y dies here
return x                    // y is undeclared from here on
```
- **Same scope, same name twice = compile error** ("already declared") —
  params, locals, function names, globals. No silent shadow-by-redeclaration.
- **Nested scope may shadow** an outer name freely; the outer scope resumes
  its own binding once the inner one closes.
- **Sequential (non-nested) blocks may reuse the same name** — each is its
  own scope, not in conflict with the other.
- A `for` loop's own induction variable dies with the loop — referencing it
  afterward is "undeclared identifier."

---

## Type grammar

```
type      ::= base postfix*
base      ::= primitive | named | fn_type | anon_struct | "(" type ")"
postfix   ::= "*" | "[" constexpr "]" | "[" "]"
named     ::= IDENT | IDENT "[" type_or_value ("," type_or_value)* "]"
fn_type   ::= "fn" "(" (type ("," type)*)? ")" type?
anon_struct ::= "struct" "{" (type IDENT)* "}"
```

Postfixes bind left-to-right over what's built so far:

```
u32*[4]     // array(4) of u32*
u32[4]*     // pointer to u32[4]
u32[2][3]   // array(2) of array(3) of u32 — leftmost dim indexes first
```

Parens are pure grouping, only needed to stop a greedy `fn` return type
from eating a trailing postfix:

```
(fn(u32) u32)[4]    // array of 4 function pointers
fn(u32) u32[4]      // one function returning u32[4]
```

Anonymous struct type (not a declaration — usable anywhere a type is):

```
struct { i32 x  i32 y } p
```
Identity is structural (keyed on field types): two anonymous structs with
the same field types are the same type. Named structs stay nominal.

---

## Pointers

```
u32* p = &x
u32 v = *p
*p = 99
u32** pp = &p
```
- Field access / indexing auto-deref: `p.field`, `arr_ptr[i]`.
- `null` is valid for any pointer type.
- `p + 1` scales by `sizeof(*p)`. `ptr - ptr` (same type) → `i64` element distance.
- `void*` arithmetic is a compile error (both directions).
- `*x` on a non-pointer is a compile error, not a raw dereference.

---

## Casts

```
(u32) x           // scalar: truncate/widen/reinterpret
(f32) i           // int -> float
(i32) f           // float -> int, truncates toward zero
(u8*) ptr         // pointer reinterpret
(fn(u32) u32) x   // integer -> function pointer
(Point){.x=1}     // cast also supplies a target type for an untyped literal
```

---

## `new` / `delete`

```
Node* n = new Node{.data = 42, .next = null}   // single object
u8* arr = new[1024] u8                          // array (count, not bytes)
delete n
delete arr
```
No GC, no RAII, no destructor call on `delete`.

---

## Structs

```
struct Point { f32 x  f32 y }
struct Config {
    i32 base_id = 100                 // field default: any constexpr
    i32 offset  = compute(10) * 2
}
struct Empty {}                        // zero-size, legal

Config c = {}                          // {} or a partial literal applies defaults
Config c2                              // bare decl — no literal, no defaults applied
```
- By-value pass/return (full copy). Use `Point*` for aliasing.
- `a == b` on two structs/arrays: field-by-field value equality.
- Implicit lanewise `==` and `!=` on same-shaped aggregates (implicit arithmetic was removed; implement `__add`, `__sub` etc. for `+ - * & | ^`). No `/ % < > <= >=`
  on aggregates, no aggregate-scalar broadcast.
- Recursive-by-value layout errors on first use (`sizeof`/instantiation), not on
  declaration. Self-pointer (`Node* next`) is always fine.

Literals:
```
Point p = {.x = 1.0, .y = 2.0}   // designated
Point p = {1.0, 2.0}             // positional, declaration order
Point p = {.x = 1.0}             // partial — rest default or zero
```

### `super` — field embedding

```
struct Base { u32 tag }
struct Derived {
    super Base info     // promotes Base's fields to Derived's top level
    u32 extra
}
```
- `d.tag` reaches the promoted field directly.
- `d.info` names the whole embedded `Base` and **aliases the promoted prefix** —
  it is the *same storage*, not a second copy. `d.tag` and `d.info.tag` are the
  same bytes; writing either is visible through the other. `sizeof(Derived)` is
  `sizeof(Base)` + Derived's own fields, with no duplication.
- `d.info` is a genuine lvalue of type `Base`: readable, writable, and copyable
  by value (`f(d.info)` passes an ordinary independent *value* copy).
- Works in `struct` and `union` bodies. Embedded type may be a generic
  param (`super T base`), resolved at instantiation.
- Method forwarding: calling a method on `Derived` (`d.method()`) automatically
  forwards to `d.info.method()` if `Derived` does not define `method` itself.
  Works across both concrete structs and generic parameter embeddings (`super T payload`).
- `(Base*)&derived_val` reads through the shared prefix (see Casts) — the same
  bytes `d.info` names.

---

## Unions

```
union Value { i32 i  f32 f  u8* s }
```
Same field grammar as struct; fields overlap at offset 0 instead of stacking.

---

## Enums

```
enum Color { u32 R  u32 G  u32 B }        // every variant has a payload
enum Option[T] { T Some  None }            // mixed payload / no-payload
enum Direction { North South East West }  // no variant has a payload
```
Layout: `u32` tag + largest payload, laid out like any struct.

Construction (contextual — target type supplies the enum). The idiom is the
designated struct-literal shape `{.Variant = payload}` — an enum variant is
stored as an ordinary field of its enum's own type, so this is the exact same
grammar `{.field = value}` already uses for structs, not a separate mechanism.
`.Variant(payload)` also works as an equivalent alternate spelling. A
payload-less variant is always bare — `.None`, never wrapped in either form,
since there's no payload to write:
```
Option[u32] o = {.Some = 42}   // idiom
Option[u32] o2 = .Some(42)     // equivalent alternate spelling
Option[u32] n = .None
```

`match` on an enum **value** (exhaustive, or `else`) — patterns use the same
two spellings, tracked identically for exhaustiveness/duplicate-arm checking:
```
match o {
    {.Some = v} { ... }   // v bound to payload
    .None       { ... }   // no payload -> no binding
}
```
A bound name in an arm is scoped to that arm only.

---

## `match`

One rule, one keyword, scrutinee is either a **value** or a **type**:

```
match scrutinee {
    PATTERN { ... }
    PATTERN { ... }
    else    { ... }      // required unless exhaustive/wildcard
}
```
Arms ordered, first match wins, no fallthrough. A known/literal thing in a
pattern position is **compared**; an unbound identifier is a **binding**.
No `else` and nothing matches = no-op (never an error). That one rule reads
the same whether the scrutinee is a value or a type — only what counts as
"a pattern" changes:

**Scrutinee is a value** — an enum, a non-float primitive, or a struct/array
(pattern = a compile-time constant literal):
```
match n {
    0 { ... }              // compared
    1 { ... }
    else { ... }
}
match opt {
    {.Some = v} { ... }    // v bound to the payload
    .None       { ... }
}
```

**Scrutinee is a type** — pattern is any type-grammar production at all
(not a fixed list), nested to any depth (`Box[Pair[A,B]*][N]` is legal).
Evaporates entirely at compile time. **Every slot in a pattern is decided
independently** — the same known-vs-unbound rule from above, applied per
identifier, not per pattern shape: `E[N]` binds both, but `E[23]` requires
the element to be a wildcard while pinning the size to exactly 23, and
`i32[N]` is the other way around — any mix is legal in any shape:
```
match T {
    P*          { }   // pointer: P bound to pointee
    E[N]        { }   // array: E = element, N = size — BOTH free here
    fn(A) B     { }   // function: A = arg type(s), B = return — BOTH free here
    Box[E]      { }   // generic, any arity: E = the type argument
    Stack[E, N] { }   // generic + const-generic together
    u32         { }   // compared, same rule as `0` above
    S           { }   // bare wildcard — binds the whole type
    struct { A; B }     { }   // struct destructure, positional on types
    impl { fn free() }  { }   // structural capability query — yes/no, no binding
    else        { }
}
```

**Field names in a struct pattern ASSERT the field's name.** A bare name after a
field type is not a label — it is a *comparison* on that field's real name, the
same known-thing-compares rule applied to names:
```
match T {
    struct { i32 count }  { }   // matches only a struct whose field 0 is named `count`
    struct { i32 }        { }   // no name -> positional: field 0 is i32, any name
    struct { E count }    { }   // E BINDS the type AND asserts the name is `count`
    struct { A; B }       { }   // `;`-separated, no names -> pure positional/structural
    struct { H; Rest... } { }   // structural peel: bind H, rebundle the tail
}
```
Because names assert, a structural peel that must match *any* field names uses the
no-name form (`struct { H; Rest... }`), not named fields.
A repeated wildcard binds consistently (`fn(A) A` matches only when arg and
return are the same type — the identical "already bound, now check" rule a
repeated value in a value pattern would need).

---

## `impl` — methods

```
struct Counter { u32 val }
impl Counter {
    fn inc()          { self.val = self.val + 1 }
    fn add(u32 n)      { self.val = self.val + n }
    fn get() u32       { return self.val }
}
```
- Desugars to `fn Counter_inc(Counter* self) {...}`. `x.m(a)` rewrites to
  `Counter_m(&x, a)` (or `Counter_m(x, a)` if `x` is already `Counter*`).
- Field lookup wins over method lookup — a method never shadows a field.
- `pub impl` exports every method; no per-method `pub`.
- Only `fn` inside `impl`. No nested types, no `const`, no static methods,
  no constructors (write an ordinary function returning the struct instead).
- Generic struct: `impl Box[T] { ... }` — params read from the struct's own
  declaration. `impl Box[u8]` (a concrete instantiation) is a compile error.
- A method may declare its own extra type params: `fn map[U](U v) U { ... }`.

---

## `alias`

```
alias Byte = u8                              // plain
alias Pair[T] = struct { T a  T b }           // generic, body is anonymous struct
alias Getter = impl { fn get() i32 }          // pattern-only (see match/impl-query)
alias Drawable = struct { (fn(void*) i32) draw }  // vtable-shaped struct
```
An alias is a parse-time expansion — the use site resolves to the underlying
type before any later pass runs; interchangeable with what it names. No
`typedef` keyword; `alias` is the one mechanism. `impl` honors an alias to a
plain struct (`impl RA` where `alias RA = Raw` attaches to `Raw`); an alias
to a generic *instantiation* is rejected the same way `impl Box[u8]` is.

---

## Generics

```
struct Box[T] { T val }
struct Pair[A, B] { A first  B second }
struct Node[T] { T data  Node[T]* next }         // self-ref via pointer OK

fn id[T](T x) T { return x }
```
- Monomorphized per distinct instantiation; no runtime type info.
- Inference: bottom-up from arguments, top-down from target type
  (decl/return/param/field/cast), left-to-right first-pin-wins with
  mismatch as a compile error.
- Explicit call-site type args work: `make[i64]()` — verified against the
  current compiler. (`specs.md` §17.2 claims this is a parse error; that
  claim did not reproduce — treat `specs.md` as stale on this point, not
  this reference.)

### Const (value) generics

```
struct Vec[T, u32 N] { T[N] e }
struct Mat[T, u32 R, u32 C] { T[R * C] cells }
```
A slot is a **value param** iff it ends in a name that isn't already a
type — `u32 N` is a value param pinned to `u32`; a bare `T` is a type
param. A value param may itself be a struct or array type, not only a
scalar, and is usable as an ordinary value inside method bodies.

A struct-typed value param can be given as an inline brace literal at the
instantiation site — no named variable needed, and its fields are readable
inside method bodies like any const-folded value:
```
struct Cfg { u32 rows  u32 cols }
struct Screen[T, Cfg C] { T[C.rows * C.cols] px }
impl Screen[T, Cfg C] {
    fn plot(u32 r, u32 c, T v) { self.px[r * C.cols + c] = v }
}
Screen[i32, {.rows=2, .cols=8}] s   // a 2x8 screen — 16 px, baked into the type
```

Two const-generic value params inferred from **different arguments and
checked against each other** — a shape-mismatched matrix multiply is a
compile-time type error, not a runtime bug:
```
struct M[T, u32 R, u32 C] { T[R * C] e }
fn matmul[T, u32 R, u32 K, u32 Cc](M[T, R, K] a, M[T, K, Cc] b) M[T, R, Cc] {
    // R, K, Cc all recovered from a's and b's own types; K must agree
    // between them or this call is a compile error, not a bad result.
    ...
}
```

### Variadic packs — `T... args` (value/call pack)

```
fn f[T](T... args) i32 { ... }
```
`T... args` bundles every trailing **call argument** into one synthesized
anonymous struct value. Peel one at a time via type-level `match`, recursing
on the tail (`Rest`) until it's empty — the base case is the zero-field
struct, not a count you track yourself:
```
fn pack_len[T](T dummy) u32 {
    match T {
        struct {} { return 0 }              // base case: no fields left
        struct { A; Rest... } {
            Rest r2 = {}                    // an empty instance of the tail type
            return 1 + pack_len(r2)          // recurse on it
        }
    }
}
```
The tail (`Rest...`) must be the last field in the pattern — it can't be
followed by anything.

### Type packs — `[Ts...]` (bracket pack)

The bracket sibling of `T... args`. Where `T... args` bundles trailing *value*
arguments written in **parens**, `Ts...` bundles trailing *type* arguments
written in **brackets** — same construction, the other argument list. It is
declared on every form that takes a generic list — `struct`, `enum`, `union`,
`alias`, and `fn`:
```
struct Def[Ts...]      { Ts field  u32 n }
enum   Sum[Ts...]      { Ts variant  None }
alias  Row[Ts...]      = Def[Ts]
fn     tuple[Ts...]()  u32 { ... }
```
`Ts` stays an ordinary **type** parameter; the `...` is a binding rule at the
application site, not a new kind. Surplus bracket arguments bundle into one
synthesized anonymous struct bound to that slot, so a single declaration serves
**every arity** — `Def[i32]`, `Def[i32, u8, f64]`, `Def[]` all instantiate the
one template. An `impl Def[T]` likewise covers every arity (bind the whole
bundle as `T`).

Three rules, all enforced at parse time:
- the pack must be the **last** generic parameter (nothing after it could ever
  receive an argument),
- **at most one** pack per generic list,
- it must be a **type** parameter — `[u32 Ns...]` (a pinned value pack) is
  rejected; there is no value for a bracket pack to bundle (see the asymmetry
  note below).

Fixed parameters may precede the pack and bind positionally; a const-generic
value parameter may too (`[u32 N, Ts...]`). Everything after the fixed prefix
bundles.

**A lone anonymous argument never collapses into the carrier.** The bundle is
built from the arguments *as written*, so arity is how many arguments you wrote,
never a function of one being anonymous:
```
Def[i32, u8, f64]        // 3 args -> carrier struct{i32,u8,f64}         -> arity 3
Def[struct{i32;u8;f64}]  // 1 arg  -> carrier struct{struct{i32,u8,f64}} -> arity 1
```
These are **different types** and are never equated.

**Destructuring** — `Def[H, Rest...]` peels the constructed carrier exactly like
`struct { H; Rest... }` peels a written anon struct (same construct). `H` binds
the first type argument, `Rest` rebundles the remainder, and `Def[Rest]`
re-applies the generic to the tail, so a walk over an argument list recurses to
the empty base case `Def[]`:
```
fn arity[X]() u32 {
    match X {
        Def[]           { return 0 }
        Def[H, Rest...] { return 1 + arity[Def[Rest]]() }   // peel + recurse on tail
        else            { return 999 }
    }
}
```
A lone type wildcard in a pack slot (`Def[E]`) binds the *whole* already-bundled
carrier, so one arm still covers every arity — that is why `Def[E]` and
`Def[H, Rest...]` mean different things, and why arm order matters (`Def[E]`
above an explicit-tail arm shadows it).

**Mixing value and type in one pattern.** A const-generic slot in a `match` on a
type is matched **as a value** — there is no mode switch or keyword. The slot's
world is decided *positionally, per slot*, from what the declaration made it: a
slot the declaration wrote as `u32 N` is a value slot; a slot written as a type
param is a type slot. So one arm can pin a value, bind a value, bind a type, and
bind a type-pack tail all at once:
```
struct Def[u32 N, Us...] { Us fields }
match X {
    Def[0, Us...]       { }   // N pinned to the VALUE 0 ; Us binds the type pack
    Def[N, Head, Rest...] { } // N value-bound ; Head type-bound ; Rest type-tail
                             // (matches every N != 0 with at least one type arg;
                             //  Rest may be empty)
    Def[N, All]         { }   // N value-bound ; All binds the WHOLE bundle
    else                { }
}
```
One arm pins a value (`0`), binds a value (`N`), binds a type (`Head`), and binds
a type-pack tail (`Rest`) — all decided per slot. This is the same "known thing
is compared, unbound identifier is bound" rule the value and type matchers
already share: value-match and type-match are not two features, they are one
matcher whose slots take their kind from the declaration. (Arms are ordered,
first match wins — a `Def[N, Head, Rest...]` with an empty `Rest` already covers
the 1-argument case, so it shadows a later `Def[N, All]`; order accordingly.)

**The asymmetry (why there is no value bracket pack).** `T... args` can bundle
values because every value carries its own type; nothing is lost. `Ts...`
bundles types, and a type has no value — so `Nums[1, 2, 3]` (values into a
bracket pack) is a category error, not a missing feature. Value-parameterize
with a const-generic before the pack (`[u32 N, Ts...]`) instead.

### Higher-kinded parameters (unapplied templates)

A generic parameter can bind a **still-generic template**, unapplied — no
`[args]` — and the body applies it later:
```
struct Box[T]    { T val }
struct HKT[M, T] { M[T] data }     // M binds a bare template; M[T] applies it

HKT[Box, i32] h                    // M = Box (unapplied), then Box[i32] inside
h.data = { .val = 5 }
```
`M` here is higher-kinded: it stands for a type *constructor*, not a type. `M[T]`
in the body is the deferred application, routed through instantiation (not read
as an array size). Arity is checked — a template applied to the wrong number of
arguments is rejected.

**Matching the head.** `match` on a higher-kinded parameter distinguishes *which*
template it is bound to — the head is a first-class thing to pattern on:
```
fn describe[M, T](HKT[M, T] h) i32 {
    match M {
        Box  { return 1 }      // M is genuinely the bare template Box
        else { return 0 }
    }
}
```
**Const-generic value under a wildcard head.** A concrete head supplies each
slot's kind from its declaration, so `Vec[E, N]` knows `N` is a value with no
annotation. A **wildcard head** (`struct M[...]`) supplies no declaration — so to
*use* a value slot as a value in the arm body, the pattern must state the value's
type, with the same type-then-value grammar a declaration uses. Two spellings,
the ordinary pin-vs-bind duality:
```
struct Vec[T, u32 N] { T[N] e }
match S {
    struct M[E, u32 N]  { /* PIN:  value-type u32 written; N usable as a value  */ }
}
struct Row[VT, T, VT N] { T[3] e }
match S {
    struct M[VT, E, VT N] { /* BIND: value-type -> VT; both VT (as type) and N   */ }
}                          /*        (as value) usable in the body               */
```
The trailing name binds the value; the type before it is its pin — checked
against the concrete (`M[E, u32 N]` will not match a `u64`-typed slot). This is
the one place in `match` where a type annotation is *required* rather than
inferred, and for the same reason a `struct` tag is required on a wildcard head:
the head cannot supply the fact, so the pattern must. A bare `struct M[E, N]`
still matches and binds `N`, but `N` cannot be read as a value without the
annotation.

### Nominal-kind tags in patterns

A pattern can pin the **nominal kind** of a slot. Position decides what the
keyword means: in a **type position** (`fn(struct Point)`, `struct Point p`) a
leading `struct`/`enum`/`union` is a redundant C-style tag on an already-known
type — accepted, carries no information. In a **match pattern** the *same*
keyword is a meaningful **kind assertion**: `struct M[X]` matches only when the
head is a struct, binds the head to `M` and its argument to `X`; `enum M[X]`
matches only enums, and so on:
```
struct Box[T] { T v }
enum   Opt[T] { T Some  None }
match S {
    enum   M[X] { }   // fires only if S's head is an ENUM  (Opt[..] yes, Box[..] no)
    struct M[X] { }   // fires only if S's head is a STRUCT (Box[..] yes, Opt[..] no)
    else        { }
}
```
Tagged applications nest — the argument of one may be another tagged application,
inner wildcards binding normally: `struct M[struct N[X]]` matches
`Box[Wrap[u64]]` with `M=Box`, `N=Wrap`, `X=u64`. Where the kind is already
known from context the tag is a no-op, so it never *hurts* to write it.

### Repeated wildcards and back-inference

A repeated wildcard name in one pattern must resolve to the same type
everywhere it appears — `match fn(u32) u32 { fn(A) A {...} }` matches
(arg and return really are both `u32`); `fn(u32) i32` would fall through.

A const-generic value parameter doesn't need to be given explicitly — it's
recovered from an already-typed argument's own instantiation:
```
struct Vec[T, u32 N] { T[N] e }
fn firstEl[T, u32 N](Vec[T, N] v) T { return v.e[0] }
firstEl(some_vec_i32_4)   // T=i32, N=4 both recovered from some_vec's own type
```

---

## `unpack` — irrefutable destructure

```
unpack {.x = px, .y = py} = point
unpack {a, b, c} = array_value
```
Sugar for a struct/array match arm that's known to always match — no
`else`, bound names escape into the *enclosing* scope (unlike a match arm).
Rejects anything refutable (a literal-pinned field, an enum-variant
pattern) — use `match` for those.

**Leaf binding.** A pattern leaf is a fresh name that binds the slot's value (a
copy). It may shadow an outer variable. There is no assign-into-an-existing-
variable form — a leaf always declares.

**`*name` — write-through binding.** A leaf written `*name` binds `name` as a
*pointer into the slot* (`name = &slot`), so `*name` reads and writes through the
live element with no copy. This is the one hardcoded pattern feature, and it is
shared by `match` and `unpack` alike (unpack is match-desugar), so `match` gets
write-through with no `match&` qualifier and no references — the pointer is the
alias, visible in the pattern:
```
unpack {.x=*px, .y=*py} = p          // px = &p.x ; *px writes through to p.x
for unpack {.x=*px} in list { *px = 100 }   // mutates each element in place
match b { {.v=*pv} { *pv = 100 } }   // write-through in a match arm, no match&
```
Write-through reaches the original only when the scrutinee is itself addressable
storage (a pointer/reference the iterator yields, a real lvalue). A by-value
scrutinee is copied into a temporary first, so `*name` then aliases the temp.

---

## `with` — declaration grouping

```
with extern {
    fn malloc(u64 n) u8*
    fn free(u8* p)
}
with const u32 { RED = 0  GREEN = 1  BLUE = 2 }
with pub const u32 { X = 100  Y = 200 }
```
Pure prefix-sharing at parse time. Prefixes: any combination of `pub`,
`extern`, `const`, plus a type for the `const TYPE { NAME = val }` form.
Doesn't nest.

---

## Function pointers

```
fn(i32, i32) i32 f = add    // declare + assign a real function
i32 r = f(3, 4)             // call through it
```
Stored in a struct field (a dispatch table), called through the field:
```
struct BinOps { fn(i32,i32) i32 add  fn(i32,i32) i32 mul }
BinOps ops = {.add = add, .mul = mul}
i32 r = ops.add(3, 4) + ops.mul(3, 4)
```
`==`/`!=` compare **identity** — same underlying function, not shape:
```
fn(i32) i32 a = f
fn(i32) i32 b = f
fn(i32) i32 c = g
bool same    = (a == b)   // true
bool differs = (a == c)   // false
```
A function pointer can be an enum variant's payload, matched and called:
```
enum OpKind { fn(i32) i32 Named  None }
OpKind o = {.Named = double}
match o {
    {.Named = h} { return h(21) }
    .None        { return 0 }
}
```
Arithmetic on a function pointer is a compile error (nothing to scale by).

---

## Control flow

```
if cond { ... } else if cond2 { ... } else { ... }
while cond { ... }
for i32 i = 0 to 10 { ... }              // exclusive end, step +1
for i32 i = 10 to 0 by (0 - 1) { ... }   // explicit step
for TYPE val in iterable { ... }         // iteration over begin/next cursor structs
for unpack PATTERN in iterable { ... }   // destructure directly in the loop header
break
continue
return expr
return                                    // void function only
defer stmt
```
`if`/`match` are statements, not expressions.

`defer` runs its statement at scope exit, LIFO order, including on early
`return` (not just normal fall-through):
```
fn f() i32 {
    defer printf("first\n")
    defer printf("second\n")
    return 0
}
// prints: second
//         first
```

---

## `sizeof` / `alignof` / `offsetof` / `nameof`

```
sizeof(x)             // u64, byte size of x's type — takes a value OR a bare type
sizeof(SomeType)      // u64, byte size of a bare type
alignof(SomeType)     // u64 — bare TYPE ONLY, not a value expression
offsetof(Struct, N)   // u64, byte offset of field index N (0-based)
nameof(SomeType)       // u8* — the type's own name, e.g. "Point"
nameof(Struct, N)     // u8* — the name of field index N
```

---

## Globals / `const`

```
u32 g_counter = 0                 // mutable global
const u32 MAX = 1024               // compile-time constant, any constexpr
const Point ORIGIN = {.x=0, .y=0}  // aggregate const also supported
```

`const X = f()` runs `f` at compile time via a full AST-walking interpreter
— the SAME interpreter, not a restricted subset. There is nothing you can
write in an ordinary function that you cannot also write in a `const`
initializer: `for`/`while`/`break`/`continue`, `new`/`delete` and real
pointer chains, `impl` methods, `super`-promoted fields, generics, `match`
on both values and types. A heap-allocated linked list, built with `new`
and traversed through `.next` pointers, folds to a single constant just
like `1 + 1` does:
```
struct Node { i32 val  Node* next }
fn build(u32 n) Node* {
    Node* head = null
    for u32 i = 0 to n {
        head = new Node{.val = (i32)i, .next = head}
    }
    return head
}
fn sum(Node* head) i32 {
    i32 total = 0
    Node* cur = head
    while cur != null {
        total = total + cur.val
        cur = cur.next
    }
    return total
}
const i32 RESULT = sum(build(10))   // folds to 45 at compile time — new,
                                     // pointers, and a while-loop traversal,
                                     // all resolved before the binary exists
fn main() i32 { return RESULT }
```

---

## Modules

```
pub fn foo() u32 { ... }      // exported
fn bar() u32 { ... }
```
No `import` keyword. `-emit-mod <path>` writes every `pub` item as a
generated interface file (structs, `extern fn` signatures, const values) —
pass that file as an ordinary input alongside its consumers.

---

## Operators

```
+  -  *  /  %              arithmetic
&  |  ^  <<  >>            bitwise
&&  ||  !                  logical (short-circuit)
==  !=  <  >  <=  >=       comparison
=  +=  -=  *=  /=  %=      assignment / compound assignment
&=  |=  ^=  <<=  >>=       bitwise compound assignment
```
`/` truncates toward zero, `%` takes dividend's sign. Unsigned narrow
overflow wraps (defined behavior).

**Assignments are addressable places (lvalues)**: `(a = b)` writes to `a` and yields `a` as an addressable place. `&(a = b)` is legal, and chained assignments like `(a = b) = c` write `c` to `a` after evaluating `b`.

**Operator overloading** (magic method names on a struct's `impl` block,
dispatched structurally — no interface declared):

```
+ - * / % & | ^ << >> == != < > <= >=   -> __add __sub __mul __div __mod
                                            __bitand __bitor __bitxor __shl __shr
                                            __eq __neq __lt __gt __lte __gte
!a  ~a                                  -> __not()  __bitnot()
a(x, ...)                               -> __call(x, ...)
a[i]                                    -> __index(i)   (T* return -> also writable, see below)
a = b                                   -> __assign(b)  (only when b doesn't already fit a's own type)
```

`__index(i) T*` (pointer INTO the container, e.g. `&self.data[i]`, not `T`)
makes `v[i]` both readable and writable — see `std/vector.t`. Known gap:
`v[i]` as a direct argument to a variadic extern call (`printf`) is unreliable;
assign to a local first. Not implemented: unary minus overload (desugars to
`0 - x` before dispatch), destructor hook for `delete`.

`__cast[T]() T` overloads `(T) x` — dispatches through `x.__cast[T]()`, one
generic method monomorphized per distinct cast target:
```
struct Meters { i32 v }
impl Meters { fn __cast[T]() T { return (T)(self.v * 100) } }
Meters m = { .v = 5 }
f64 cm  = (f64) m   // -> m.__cast[f64]()
i32 icm = (i32) m   // -> m.__cast[i32](), same method, different instantiation
```
A non-generic `__cast() T` (fixed return type) only applies when the cast's
own target type matches that fixed `T` — casting to anything else does not
dispatch through it.

`__delete() void` overloads `delete x` — called automatically before the
underlying `free`. This serves as the language's real destructor hook.
```
impl Vector[T] {
    fn __delete() void {
        delete self.data
    }
}
// delete v  ->  calls v.__delete() then frees v
```

---

## Showcases

Every construct above is shown in isolation. These compose several at once.

**Function pointer returning a generic struct, called and field-accessed
inline:**
```
struct Box[T] { T val }
struct Node { fn() Box[u32] get_box }
fn make_box() Box[u32] { return {.val = 99} }
fn main() i32 {
    Node n = {.get_box = make_box}
    return (i32) n.get_box().val
}
```

**Triple pointer to an array of enums, dereferenced and matched:**
```
enum State { u32 Active  bool Inactive }
fn main() i32 {
    State[2] arr = { {.Active = 42}, {.Inactive = true} }
    State[2]* p1 = &arr
    State[2]** p2 = &p1
    State[2]*** p3 = &p2
    match (***p3)[0] {
        {.Active = v}   { return (i32) v }
        {.Inactive = b} { return -1 }
    }
    return -2
}
```

**Nested `match`, unwrapping two recursive calls in one arm:**
```
enum Option[T] { T Some  None }
fn fib(u32 n) Option[u32] {
    if n < 2 { return {.Some = n} }
    match fib(n - 1) {
        {.Some = a} {
            match fib(n - 2) {
                {.Some = b} { return {.Some = a + b} }
                .None       { return .None }
            }
        }
        .None { return .None }
    }
    return .None
}
```

**Literal-guarded pattern**: a literal in one field position pins it to a
constant while another position in the same pattern still binds freely —
first-match-wins arm order decides which shape fires:
```
struct Vec { i32[2] xy }
enum Shape { Vec Line  None }
fn desc(Shape s) i32 {
    match s {
        {.Line = {.xy = {0, y}}} { return y }       // first elem pinned to 0
        {.Line = {.xy = {x, y}}} { return x + y }    // falls through otherwise
        .None { return -1 }
    }
}
```

**Compile-time recursive walk over a struct's own field list** — no runtime
reflection, `match` on the *type*, peeling one field at a time via
pack-tail destructuring:
```
extern fn printf(u8* fmt, ...) i32
struct Pair[T] { T a  T b }
struct Widget { u8 tag  Pair[i32] data }
fn dump[Orig, Walk, u32 N](i32 depth) void {
    match Walk {
        struct { H; Rest... } {                  // no-name form: names would ASSERT
            printf("%s %s\n", nameof(H), nameof(Orig, N))
            dump[Orig, Rest, N + 1](depth)
        }
        struct {} {}
    }
}
fn main() i32 { dump[Widget, Widget, 0](0)  return 0 }
```

**A hand-built existential**: an opaque pointer plus a vtable built from a
variadic pack of per-type thunks — no language feature named "interface" or
"trait" anywhere in it:
```
struct Dyn[V] { void* obj  V vt }
fn dyn[T, V](T* o, V... fns) Dyn[V] { return { .obj = (void*)o, .vt = fns } }
alias Getter = struct { (fn(void*) i32) get }
fn t_get[T](void* p) i32 { T* o = (T*)p  return o.get() }
struct A { i32 v }
impl A { fn get() i32 { return 3 } }
struct B { u8* s }
impl B { fn get() i32 { return 7 } }
fn main() i32 {
    A a = { .v = 0 }
    B b = { .s = "x" }
    Dyn[Getter][2] items
    items[0] = dyn[A, Getter](&a, t_get[A])
    items[1] = dyn[B, Getter](&b, t_get[B])
    i32 sum = 0
    for u32 i = 0 to 2 { sum = sum + items[i].vt.get(items[i].obj) }
    return sum   // 10
}
```
The same dispatch function can instead be a **const-generic value
parameter** baked into the type itself — `sizeof` the result is then just
the data pointer (8 bytes): the "virtual call" is fully resolved at compile
time, nothing stored at runtime.
```
struct Circle { i32 r }
impl Circle { fn area() i32 { return 42 } }
fn t_area[T](void* p) i32 { T* o = (T*)p  return o.area() }
struct DynA[fn(void*) i32 F] { void* obj }
impl DynA[F] { fn area() i32 { return F(self.obj) } }
fn main() i32 {
    Circle c = { .r = 1 }
    DynA[t_area[Circle]] d = { .obj = (void*)&c }
    return (i32)sizeof(d)   // 8
}
```

Structural capability check **inside a generic function body**, choosing at
compile time between a real thunk and a null vtable entry:
```
fn as_getter[T](T* o) Dyn[Getter] {
    match T {
        impl { fn get() i32 } { return dyn[T, Getter](o, t_get[T]) }
        else { return { .obj = null, .vt = { .get = null } } }
    }
}
```

`super` combined with the vtable pattern: a `Derived*` upcast to `Base*`
dispatches through `Base`'s own method and reads the shared field prefix:
```
struct Base { i32 x }
impl Base { fn get() i32 { return self.x } }
struct Derived { super Base base  i32 extra }
impl Derived { fn get() i32 { return self.x + self.extra } }
fn main() i32 {
    Derived d
    d.x = 20   // promoted field, offset 0 — same offset Base.x has
    d.extra = 3
    Base* upcast = (Base*)&d
    return d.get() + upcast.get()   // 23 + 20 = 43
}
```

**A generic enum whose variants are mixed** (one payload is the type
parameter, the other is a fixed concrete type — not every variant has to be
generic), **nested inside another generic enum as an array payload**,
matched two levels deep, calling a function pointer inside the inner arm:
```
enum Res[T] { T Ok  i32 Err }
enum Node[T] { Res[T][2] Branch  T Leaf }
fn sum_node[T](Node[T] n, fn(T) i32 f) i32 {
    match n {
        {.Branch = b} {
            i32 acc = 0
            for u32 i = 0 to 2 {
                match b[i] {
                    {.Ok = v}  { acc = acc + f(v) }
                    {.Err = e} { acc = acc + e }
                }
            }
            return acc
        }
        {.Leaf = v} { return f(v) }
    }
    return 0
}
fn id(i32 x) i32 { return x }
fn main() i32 {
    Res[i32][2] arr = { {.Ok = 10}, {.Err = 5} }
    return sum_node({.Branch = arr}, id)   // f(10) + 5 = 15
}
```

**Two const-generic value parameters at once**, a real 2D matrix, with an
`impl` method returning a new instance of the same generic instantiation:
```
struct Mat[T, u32 R, u32 C] { T[R][C] d }
impl Mat[T, R, C] {
    fn add(Mat[T, R, C]* other) Mat[T, R, C] {
        Mat[T, R, C] res
        for u32 i = 0 to R {
            for u32 j = 0 to C { res.d[i][j] = self.d[i][j] + other.d[i][j] }
        }
        return res
    }
}
fn main() i32 {
    Mat[i32, 2, 3] m1
    m1.d[0][0]=1  m1.d[0][1]=2  m1.d[0][2]=3
    m1.d[1][0]=4  m1.d[1][1]=5  m1.d[1][2]=6
    Mat[i32, 2, 3] m2
    m2.d[0][0]=10  m2.d[0][1]=20  m2.d[0][2]=30
    m2.d[1][0]=40  m2.d[1][1]=50  m2.d[1][2]=60
    Mat[i32, 2, 3] m3 = m1.add(&m2)
    return m3.d[1][2]   // 66
}
```

**`void` as a genuine zero-sized unit type** — not a dedicated feature, just an
ordinary `Type*` that inherits every mechanism ordinary types get. A fallible
operation with no success payload costs nothing extra to express, and `{}`
already supplies the one unit value with no dedicated literal syntax needed:
```
extern fn printf(u8* fmt, ...) i32
enum Result[T, E] { T Ok  E Err }
struct Config { i32 x }
fn validate(Config* c) Result[void, u8*] {
    if c.x <= 0 { return {.Err = "bad config"} }
    return {.Ok = {}}          // {} supplies the void payload
}
fn main() i32 {
    Config good = {.x = 1}
    Config bad = {.x = 0}
    match validate(&good) {
        {.Ok = v}  { printf("good: ok, sizeof(v)=%d\n", (i32)sizeof(v)) }   // 0
        {.Err = e} { printf("good: err %s\n", e) }
    }
    match validate(&bad) {
        {.Ok = v}  { printf("bad: ok\n") }
        {.Err = e} { printf("bad: err %s\n", e) }   // "bad config"
    }
    return 0
}
```

---

## CLI

```
./torrent file.t                    # JIT: compile + run main()
./torrent -c file.t -o out.o         # AOT: emits a RELOCATABLE object, not a runnable binary
gcc -o out aot_shim.c out.o          # link with the C entry-point shim to get a runnable binary
./torrent a.t b.t                    # multiple files, one compilation
./torrent file.t -- arg1 arg2        # args after -- go to your program's argv
./torrent -emit-mod out.tmod file.t  # write pub interface file
```
""