// HashMap[K, V] - open-addressing hash map, linear probing, tombstone deletes.
//
// Manual ownership: a HashMap[K, V] owns its backing slots array, exactly
// like Vector owns .data. Same rule applies -- __delete() is never called by
// hand, only via:
//
//   delete p      one heap object
//   delete[] p    a heap array (destroys every element, then frees)
//   scope exit    RAII, for any local
//
// Element ownership is the SAME single predicate Vector and Array use --
// does V have __delete()? -- asked in the same two places:
//
//   store_elem(idx, item)   V comes IN.  Owning V -> item.copy(), a real
//                           deep copy. Plain V -> a field-copy.
//   move_out(idx)           V goes OUT. Owning V -> the slot's value is
//                           blanked after, so it stops aliasing what left.
//                           Plain V -> the blank is free, nothing to leak.
//
// Only V is checked against Owning, not K: keys are compared with == to find
// a slot and never handed back to the caller, so an owning K would leak
// regardless -- same restriction implied (not yet enforced) in Vector for
// keys used this way. Not solved here; see note on remove()/rehash() below.
//
// state: 0 = empty, 1 = live, 2 = tombstone. A tombstone keeps the probe
// chain intact for later lookups but must not itself be treated as live by
// copy()/__delete()/iteration.
struct Entry[K, V] { K key  V value  u8 state }

// The public key/value pair. Entry carries a third `state` byte that is pure
// bookkeeping (empty/live/tombstone), and a literal written against Entry would
// force a caller to supply it -- `{1, 10, 0}`, where the 0 means nothing to
// them. Pair is what an initializer literal is checked against instead, so the
// spelling is `{ {1, 10}, {2, 20} }` and the internal state stays internal.
pub struct Pair[K, V] { K key  V value }

// Field defaults ARE the empty state. Storage is not zeroed (neither `new` nor
// a local declaration clears it), so a bare `HashMap[K, V] m` would otherwise
// hold a garbage slots pointer and a garbage capacity -- and there would be no
// way to tell "never constructed" from "in use". Declaring the defaults here
// makes `HashMap[K, V] m` a valid empty map that maybe_grow() then allocates on
// first use, with no constructor call required.
pub struct HashMap[K, V] {
    Entry[K, V]* slots = null
    u32 capacity = 0
    u32 size = 0
    u32 tombstones = 0
}

pub impl HashMap[K, V] {
    // ANY type is usable as a key. Resolution is structural and ordered:
    //
    //   1. a user-written __hash() wins outright (Hashable), so a type that
    //      needs a specific hash -- or hashes only part of itself -- says so
    //      once and every container honours it;
    //   2. u8* is hashed and compared BY CONTENT, not by address. This is the
    //      case that silently broke: two identical string literals are two
    //      different pointers, so `m["hello"] = 1` then `m["hello"]` inserted a
    //      second entry and read back 0;
    //   3. scalars mix their bits;
    //   4. anything else is peeled field-by-field via the structural matcher
    //      and each field hashed by these same rules, recursively.
    //
    // Field-by-field rather than hashing sizeof(K) raw bytes: a struct's
    // padding is not part of its value, and a pointer field must be followed
    // rather than hashed as an address. The empty struct terminates the walk.
    static fn hash_key(K k) u32 {
        match K {
            Hashable { return k.__hash() }
            u8* {
                u32 h = 2166136261
                u32 i = 0
                while k[i] != 0 {
                    h = (h ^ (u32)k[i]) * 16777619
                    i = i + 1
                }
                return h
            }
            u32  { return k * 2654435761 }
            i32  { return (u32)k * 2654435761 }
            u64  { return ((u32)k ^ (u32)(k >> 32)) * 2654435761 }
            i64  { return ((u32)k ^ (u32)((u64)k >> 32)) * 2654435761 }
            u16  { return (u32)k * 2654435761 }
            i16  { return (u32)k * 2654435761 }
            u8   { return (u32)k * 2654435761 }
            i8   { return (u32)k * 2654435761 }
            bool { return (u32)k * 2654435761 }
            struct {} { return 2166136261 }
            struct { H; Rest... } {
                unpack {head, rest...} = k
                u32 hh = HashMap[H, V].hash_key(head)
                u32 rh = HashMap[Rest, V].hash_key(rest)
                return (hh ^ rh) * 16777619
            }
            else { return 0 }
        }
    }

    // Key equality, resolved by the same rules hash_key uses -- the two MUST
    // agree, or a lookup hashes to the right slot and then fails to recognise
    // the key sitting in it. `==` on u8* compares addresses, so string keys
    // need an explicit content comparison here for the same reason they need
    // one above.
    static fn keys_equal(K a, K b) bool {
        match K {
            u8* {
                u32 i = 0
                while a[i] != 0 {
                    if a[i] != b[i] { return false }
                    i = i + 1
                }
                return b[i] == 0
            }
            // Peeled field-by-field for the same reason hash_key is, and it
            // must peel wherever hash_key peels or the two disagree. A plain
            // `a == b` cannot serve here: lanewise struct comparison is
            // rejected outright once any field is a pointer, so a key like
            // `struct { Inner i  u8* name }` had no working equality at all --
            // and a string FIELD would have compared by address even if it did.
            struct {} { return true }
            struct { H; Rest... } {
                unpack {ah, arest...} = a
                unpack {bh, brest...} = b
                if !HashMap[H, V].keys_equal(ah, bh) { return false }
                return HashMap[Rest, V].keys_equal(arest, brest)
            }
            else { return a == b }
        }
    }

    // A freshly allocated slots array with every slot explicitly marked empty.
    //
    // `new` does NOT zero its allocation, so a recycled block arrives holding
    // whatever the previous owner left. state is the probe loop's only notion of
    // "is this slot live", so an uninitialized byte that happens to be 1 makes a
    // stale slot look occupied: lookups then match a garbage key, or stop early
    // on a chain that should have continued. Marking it here is the one place
    // that has to know, rather than every reader defending against junk.
    static fn fresh_slots(u32 n) Entry[K, V]* {
        Entry[K, V]* s = new[n] Entry[K, V]
        for u32 i = 0 to n {
            s[i].state = 0
        }
        return s
    }

    static fn with_capacity(u32 cap = 16) HashMap[K, V] {
        u32 c = cap
        if c == 0 { c = 16 }
        return {
            .slots = HashMap[K, V].fresh_slots(c),
            .capacity = c,
            .size = 0,
            .tombstones = 0
        }
    }

    static fn create() HashMap[K, V] { return HashMap[K, V].with_capacity() }

    // Per-map step function baked into IteratorCursor's const-generic type.
    // Walks every slot, skipping empty (0) and tombstone (2) states -- only
    // state==1 slots are yielded. Same forward-reference ordering rule as
    // Vector/Array: step() must be declared before begin() in this impl
    // block or the static resolves to a null symbol.
    // Yields the whole ENTRY, not just the value, so iteration can see keys:
    //
    //     for unpack {k, v} in map { ... }
    //
    // Entry's first two fields are `key` and `value`, and a pattern may bind a
    // PREFIX of a struct's fields, so `{k, v}` destructures a key/value pair and
    // leaves the internal `state` byte unmentioned. Yielding V* instead made
    // `for unpack {k, v}` impossible -- there was no pair to destructure, only a
    // bare value.
    static fn step(struct{Entry[K, V]* data  u32 pos  u32 len}* s) Option[Entry[K, V]*] {
        while s.pos < s.len {
            u32 cur = s.pos
            s.pos = s.pos + 1
            if s.data[cur].state == 1 {
                return .Some(&s.data[cur])
            }
        }
        return .None
    }

    // Single source of truth for "how does a V get stored into this slot":
    // deep-copy if V owns a resource, plain value otherwise. insert() and
    // rehash()'s direct-write path both route through this.
    fn store_elem(u32 index, V item) void {
        match V {
            Owning { self.slots[index].value = item.copy() }
            else   { self.slots[index].value = item }
        }
    }

    // store_elem's counterpart: takes a V OUT of a slot and blanks the
    // slot's value behind it, marking it a tombstone. For a plain V the
    // blank is free; for an owning V it is load-bearing -- without it the
    // tombstoned slot keeps a live second alias to whatever the moved-out
    // V owns, and a later destroy walk (__delete or copy) double-frees
    // through it. remove() is this map's one "move a V out" operation.
    fn move_out(u32 index) V {
        V out = self.slots[index].value
        V blank
        self.slots[index].value = blank
        self.slots[index].state = 2
        return out
    }

    fn find_slot(K key) struct{u32 idx  bool found} {
        u32 h = HashMap[K, V].hash_key(key)
        u32 idx = h % self.capacity
        u32 first_tomb = self.capacity
        bool has_tomb = false
        u32 i = 0
        while i < self.capacity {
            u8 st = self.slots[idx].state
            if st == 0 {
                if has_tomb { return {.idx = first_tomb, .found = false} }
                return {.idx = idx, .found = false}
            }
            if st == 1 {
                if HashMap[K, V].keys_equal(self.slots[idx].key, key) { return {.idx = idx, .found = true} }
            }
            if st == 2 {
                if !has_tomb { first_tomb = idx  has_tomb = true }
            }
            idx = (idx + 1) % self.capacity
            i = i + 1
        }
        return {.idx = self.capacity, .found = false}
    }

    // Rebuilds into a fresh table of `new_cap` slots, re-inserting every
    // live (state==1) entry directly -- NOT via self.insert(), which would
    // re-run maybe_grow() and re-check the load factor on every single
    // re-insert for no reason (size/tombstones are already reset to 0
    // before the loop starts, so it can't actually re-trigger a grow, but
    // it's wasted work and the wrong entry point: insert() decides "found
    // vs not found", and every key here is already known-unique). Keys are
    // moved as plain values (find_slot's == already means K is comparable,
    // not typically an owning type); values route through store_elem so an
    // owning V is deep-copied into its new slot rather than aliased twice.
    fn rehash(u32 new_cap) void {
        Entry[K, V]* old_slots = self.slots
        u32 old_cap = self.capacity
        self.slots = HashMap[K, V].fresh_slots(new_cap)
        self.capacity = new_cap
        self.size = 0
        self.tombstones = 0
        u32 i = 0
        while i < old_cap {
            if old_slots[i].state == 1 {
                unpack {idx, found} = self.find_slot(old_slots[i].key)
                self.slots[idx].key = old_slots[i].key
                self.store_elem(idx, old_slots[i].value)
                self.slots[idx].state = 1
                self.size = self.size + 1
            }
            i = i + 1
        }
        delete[] old_slots
    }

    fn maybe_grow() void {
        // A map that was never constructed (`HashMap[K, V] m` on its own, or
        // one initialized straight from a literal) has capacity 0 and a null
        // slots pointer. Give it a real table before anything indexes into it:
        // without this, `capacity * 2` is still 0, every probe divides by zero,
        // and the first insert dereferences null.
        if self.capacity == 0 {
            self.rehash(16)
            return
        }
        // Grow past 70% load (counting tombstones too, since they occupy
        // probe-chain slots just as much as live entries do).
        u32 used = self.size + self.tombstones
        if used * 10 >= self.capacity * 7 {
            self.rehash(self.capacity * 2)
        }
    }

    fn insert(K key, V value) bool {
        self.maybe_grow()
        unpack {idx, found} = self.find_slot(key)
        if found {
            // Overwriting a live slot's value is itself a store, not a
            // move -- the old value must be destroyed before being
            // replaced, same as Vector.set() moves the displaced element
            // out first so scope-exit RAII destroys it. Do that here too
            // rather than clobbering an owning V's old resource in place.
            V old = self.move_out(idx)
            self.slots[idx].state = 1
            self.store_elem(idx, value)
            return false
        }
        self.slots[idx].key = key
        self.store_elem(idx, value)
        self.slots[idx].state = 1
        self.size = self.size + 1
        return true
    }

    // Retrieves a COPY of the value by key, wrapped in Option. Same shallow-
    // copy caveat as Vector.get()/pop(): the returned V is a raw field-copy,
    // not routed through store_elem. Safe to READ through a write-through
    // bind ({.Some = *v}), never safe as a second owner -- mutating or
    // dropping it independently aliases (and can double-free) the slot's
    // real buffer. For in-place mutation of an owning V, use get_ptr()
    // instead, the same way Vector callers reach for `v[i]` (not `v.get(i)`)
    // when they need a real handle into storage.
    fn get(K key) Option[V] {
        unpack {idx, found} = self.find_slot(key)
        if found { return {.Some = self.slots[idx].value} }
        return .None
    }

    // Pointer into the live slot's value, or .None if the key isn't present.
    // This is HashMap's equivalent of Vector.__index() / Array.__index():
    // a real address into self.slots, not a copy -- safe to mutate an
    // owning V in place through it (m.get_ptr(k) unwrapped, then push()/
    // field-writes go straight to the stored value, no aliasing risk).
    fn get_ptr(K key) Option[V*] {
        unpack {idx, found} = self.find_slot(key)
        if found { return {.Some = &self.slots[idx].value} }
        return .None
    }

    // `m[key]` -- a pointer into the slot's value, so the result is both
    // readable and writable, exactly like Vector.__index()/Array.__index().
    //
    // A missing key is INSERTED with a zero value and its address returned,
    // which is what makes `m[key] = v` work as a genuine insert rather than
    // only an overwrite. That is C++'s operator[] semantics, and it is chosen
    // for the same reason: without it, assignment through the index operator
    // could only ever update keys that already existed, and every insertion
    // would have to go through insert() instead.
    //
    // The consequence to know: a bare READ of a missing key (`m[k]` on the
    // right-hand side) also inserts it, growing the map. Use get()/contains()
    // when the lookup must not mutate -- again the same trade C++ makes.
    fn __index(K key) V* {
        self.maybe_grow()
        unpack {idx, found} = self.find_slot(key)
        if !found {
            self.slots[idx].key = key
            self.slots[idx].state = 1
            self.size = self.size + 1
        }
        return &self.slots[idx].value
    }

    fn remove(K key) Option[V] {
        unpack {idx, found} = self.find_slot(key)
        if !found { return .None }
        self.size = self.size - 1
        self.tombstones = self.tombstones + 1
        return {.Some = self.move_out(idx)}
    }

    fn contains(K key) bool {
        unpack {idx, found} = self.find_slot(key)
        return found
    }

    // Deep copy: allocates a new slots array and copies every live entry.
    // Routes through store_elem, so an owning V gets its own resource per
    // entry instead of aliasing the source's. Tombstones are NOT carried
    // over -- the copy starts clean, same as Vector.copy() only copies
    // [0, size) rather than the whole capacity.
    fn copy() HashMap[K, V] {
        HashMap[K, V] out = HashMap[K, V].with_capacity(self.capacity)
        u32 i = 0
        while i < self.capacity {
            if self.slots[i].state == 1 {
                unpack {idx, found} = out.find_slot(self.slots[i].key)
                out.slots[idx].key = self.slots[i].key
                out.store_elem(idx, self.slots[i].value)
                out.slots[idx].state = 1
                out.size = out.size + 1
            }
            i = i + 1
        }
        return out
    }

    // Frees the backing slots array. delete[] destroys every Entry first
    // when Entry has a destructor -- but Entry itself never declares
    // __delete() here (it's a plain struct), so a V's destructor is not
    // reached by delete[] alone the way Vector's element destruction is:
    // Vector's T sits directly in the array, HashMap's V sits inside a
    // wrapping Entry[K, V] struct that owns nothing itself.
    //
    // So each live V is moved out into an ordinary local first -- __delete()
    // is never called by name (see file header), the same rule Vector's own
    // __delete() and clear() both honor. move_out() blanks the slot and the
    // local's scope-exit RAII (the closing brace of the inner block) does
    // the actual destruction, exactly like Vector.clear() destroys a
    // displaced element via set()/move_out rather than naming __delete().
    fn __delete() void {
        if self.slots != null {
            match V {
                Owning {
                    u32 i = 0
                    while i < self.capacity {
                        if self.slots[i].state == 1 {
                            { V v = self.move_out(i) }   // dies at this brace
                        }
                        i = i + 1
                    }
                }
                else {}
            }
            delete[] self.slots
            self.slots = null
        }
        self.capacity = 0
        self.size = 0
        self.tombstones = 0
    }

    fn len() u32 { return self.size }
    fn cap() u32 { return self.capacity }
    fn is_empty() bool { return self.size == 0 }

    // Whole-map initialization from a literal of key/value pairs:
    //
    //     HashMap[i32, i32] m = { {1, 10}, {2, 20} }
    //
    // The literal has no type of its own -- what `{1, 10}` MEANS is decided
    // entirely by the target, which here is this method's declared parameter.
    // N is inferred from the literal's own element count.
    fn __assign[u32 N](Pair[K, V][N] pairs) void {
        for u32 i = 0 to N {
            unpack {k, v} = pairs[i]
            self.insert(k, v)
        }
    }

    // Iterator support. Yields live ENTRIES, so both spellings work:
    //
    //     for Entry[K, V]* e in map { ... }       // e.key, e.value
    //     for unpack {k, v} in map { ... }        // destructured pair
    //
    // Same "state baked into the const-generic step function" pattern
    // Vector/Array use -- there is no map-specific cursor implementation here.
    fn begin() IteratorCursor[Entry[K, V], struct{Entry[K, V]* data  u32 pos  u32 len}, HashMap[K, V].step] {
        return {
            .state = {
                .data = self.slots,
                .pos = 0,
                .len = self.capacity
            }
        }
    }
}