#ifndef VM_ANON_H
#define VM_ANON_H
#include "vm/vm.h"
struct page;
enum vm_type;

struct anon_page {
/* -------------------------------------------------------- PROJECT3 : Virtual Memory - Swap In/Out -------------------------------------------------------- */
    uint32_t slot_no; // swap out될 때 이 페이지가 저장된 slot의 번호
/* -------------------------------------------------------- PROJECT3 : Virtual Memory - Swap In/Out -------------------------------------------------------- */
};

void vm_anon_init (void);
bool anon_initializer (struct page *page, enum vm_type type, void *kva);

#endif
