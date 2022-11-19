# Général
- Optimize
  - Clock ?
  - cleanup à la fin
  - voir si vraiment besoin lock pour seg list ?
  - fixer segfault pour grosses valeurs
- 


Usage des lockstamp:
- acquire_locks: 1 check si locked puis lock si pas lock => c&s ?
- release_locks: (force) unlock & (force) set version => set version (simple ? atomic ?) puis c&s (hypothèse: déjà lock de toute façon)
- rset_check: besoin de lock & version => c&s sur le lock, read version ?
- tm_read: pre : sample le lock & vesrion => c&s sur le lock, read version
- post: resample lock & version
- 
- 


tm_create:
- zero first seg
- alignment for every segment
- need concurrent-safe

tm_destroy:
- guarantee: no running transac, not destroyed yet, not concurrent on same shared
- must free first seg & all non-freed yet

tm_start:
- need concurrent-safe
- Never fail, must not be NULL

tm_size:
- idem
- must be a const ?

tm_align:
- idem

tm_begin: -> tx id
- need concurrent-safe
- no nested
- (1st implem) drop if concurrent ?
- guarantee: is _ro, read() only

tm_end: -> bool (whole commited OK)
- concurrent only on different mem or different tx ID
- not called if any transac notified abort

tm_read:
- concurrent with different mem or tx ID only
- some undefined behav possible ?

tm_write:
- concurrent with different mem or tx ID only

tm_alloc:
- concurrent with different mem or tx ID only
- nomem_alloc => no abort
- zero init

tm_free:
- concurrent with different mem or tx ID only
- ensure not free first seg ?


