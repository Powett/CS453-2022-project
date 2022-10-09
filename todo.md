# Général
- [ ] Helper files organisation
- [ ] safen allocs list ?
- [ ] ReadSet
- [ ] WriteSet
- [ ] Passer la size dans segment, passer start en segment*
# Fonctions
- [ ] tm_begin
- [ ] tm_end
- [ ] tm_read
  - [ ] Opti de parcourir toute la liste ?
- [ ] tm_write
- [ ] tm_alloc
- [ ] tm_dealloc

 Questions:

 - Si read mais ensuite transac fail, il faut revert le read ?
 - Si alloc mais ensuite transac fail, il faut free le segment ?
 - 