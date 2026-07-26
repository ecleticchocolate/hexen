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

extern fn printf(u8* fmt, ...) i32

struct Entry[K, V] { K key  V value  u8 state }

pub struct HashMap[K, V] {
    Entry[K, V]* slots
    u32 capacity
    u32 size
    u32 tombstones
}

pub impl HashMap[K, V] {
    static fn hash_key(K k) u32 {
        match K {
            u32 { return k * 2654435761 }
            i32 { return (u32)k * 2654435761 }
            else { return 0 }
        }
    }

    static fn with_capacity(u32 cap = 16) HashMap[K, V] {
        u32 c = cap
        if c == 0 { c = 16 }
        return {
            .slots = new[c] Entry[K, V],
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
    static fn step(struct{Entry[K, V]* data  u32 pos  u32 len}* s) Option[V*] {
        while s.pos < s.len {
            u32 cur = s.pos
            s.pos = s.pos + 1
            if s.data[cur].state == 1 {
                return .Some(&s.data[cur].value)
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
                if self.slots[idx].key == key { return {.idx = idx, .found = true} }
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
        self.slots = new[new_cap] Entry[K, V]
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

    // Iterator support for `for TYPE val in map { ... }` / `for unpack ...`.
    // Yields live VALUES only (no key), same "state baked into the const-
    // generic step function" pattern Vector/Array use -- there is no
    // map-specific cursor implementation here.
    fn begin() IteratorCursor[V, struct{Entry[K, V]* data  u32 pos  u32 len}, HashMap[K, V].step] {
        return {
            .state = {
                .data = self.slots,
                .pos = 0,
                .len = self.capacity
            }
        }
    }
}