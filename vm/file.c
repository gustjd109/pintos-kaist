/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"

/* -------------------------------------------------------- PROJECT3 : Virtual Memory - Memory Mapped Files -------------------------------------------------------- */
#include "include/threads/vaddr.h"
#include "include/userprog/process.h"
/* -------------------------------------------------------- PROJECT3 : Virtual Memory - Memory Mapped Files -------------------------------------------------------- */

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* The initializer of file vm */
void
vm_file_init (void) {
}

/* Initialize the file backed page */
/* file-backed page를 초기화하는 함수 */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &file_ops;

	struct file_page *file_page = &page->file;
/* -------------------------------------------------------- PROJECT3 : Virtual Memory - Memory Mapped Files -------------------------------------------------------- */
    struct lazy_load_arg *lazy_load_arg = (struct lazy_load_arg *)page->uninit.aux;
    file_page->file = lazy_load_arg->file;
    file_page->ofs = lazy_load_arg->ofs;
    file_page->read_bytes = lazy_load_arg->read_bytes;
    file_page->zero_bytes = lazy_load_arg->zero_bytes;
    return true;
/* -------------------------------------------------------- PROJECT3 : Virtual Memory - Memory Mapped Files -------------------------------------------------------- */
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;
/* -------------------------------------------------------- PROJECT3 : Virtual Memory - Swap In/Out -------------------------------------------------------- */
    return lazy_load_segment(page, file_page);
/* -------------------------------------------------------- PROJECT3 : Virtual Memory - Swap In/Out -------------------------------------------------------- */
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
/* -------------------------------------------------------- PROJECT3 : Virtual Memory - Swap In/Out -------------------------------------------------------- */
    if (pml4_is_dirty(thread_current()->pml4, page->va))
	{
		file_write_at(file_page->file, page->va, file_page->read_bytes, file_page->ofs);
		pml4_set_dirty(thread_current()->pml4, page->va, 0);
	}

	// 페이지와 프레임의 연결 끊기
	page->frame->page = NULL;
	page->frame = NULL;
	pml4_clear_page(thread_current()->pml4, page->va);
	return true;
/* -------------------------------------------------------- PROJECT3 : Virtual Memory - Swap In/Out -------------------------------------------------------- */
}

/* Destory the file backed page. PAGE will be freed by the caller. */
/* file-backed page를 삭제하는 함수 */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
/* -------------------------------------------------------- PROJECT3 : Virtual Memory - Memory Mapped Files -------------------------------------------------------- */
	if (pml4_is_dirty(thread_current()->pml4, page->va))
    {
        file_write_at(file_page->file, page->va, file_page->read_bytes, file_page->ofs);
        pml4_set_dirty(thread_current()->pml4, page->va, 0);
    }
    pml4_clear_page(thread_current()->pml4, page->va);
/* -------------------------------------------------------- PROJECT3 : Virtual Memory - Memory Mapped Files -------------------------------------------------------- */
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {
/* -------------------------------------------------------- PROJECT3 : Virtual Memory - Memory Mapped Files -------------------------------------------------------- */
	struct file *f = file_reopen(file); // 파일에 대한 새로운 파일 디스크립터를 얻어 다른 매핑에 영향을 주거나 영향을 받지 않는 독립적은 매핑 가지도록 하기 위해 file_reopen() 함수 사용
    void *start_addr = addr; // 매핑 성공 시 파일이 매핑된 가상 주소 반환하는 데 사용
    
    // 이 매핑을 위해 사용한 총 페이지 수 계산
    int total_page_count = length <= PGSIZE ? 1 : length % PGSIZE ? length / PGSIZE + 1 : length / PGSIZE;

    // 주어진 파일 길이와 length를 비교해서 length보다 file 크기가 작으면 파일 통으로 싣고 파일 길이가 더 크면 주어진 length만큼만 load
    size_t read_bytes = file_length(f) < length ? file_length(f) : length;
    size_t zero_bytes = PGSIZE - read_bytes % PGSIZE;

    ASSERT((read_bytes + zero_bytes) % PGSIZE == 0); // read_bytes + zero_bytes한 값이 PGSIZE로 나누어 떨어지는지 확인
    ASSERT(pg_ofs(addr) == 0); // upage가 페이지 정렬되어 있는지 확인
    ASSERT(offset % PGSIZE == 0); // ofs가 페이지 정렬되어 있는지 확인

    // 파일을 페이지 단위로 잘라 해당 파일의 정보들을 lazy_load_arg 구조체에 저장
    while (read_bytes > 0 || zero_bytes > 0)
    {
        size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
        size_t page_zero_bytes = PGSIZE - page_read_bytes;

        struct lazy_load_arg *lazy_load_arg = (struct lazy_load_arg *)malloc(sizeof(struct lazy_load_arg));
        lazy_load_arg->file = f;
        lazy_load_arg->ofs = offset;
        lazy_load_arg->read_bytes = page_read_bytes;
        lazy_load_arg->zero_bytes = page_zero_bytes;

        // vm_alloc_page_with_initializer() 함수를 호출하여 대기 중인 객체를 생성
        if (!vm_alloc_page_with_initializer(VM_FILE, addr, writable, lazy_load_segment, lazy_load_arg))
            return NULL;

        struct page *p = spt_find_page(&thread_current()->spt, start_addr);
        p->mapped_page_count = total_page_count;

        /* Advance. */
        // 읽은 바이트와 0으로 채운 바이트를 추적하고 가상 주소를 증가
        read_bytes -= page_read_bytes;
        zero_bytes -= page_zero_bytes;
        addr += PGSIZE;
        offset += page_read_bytes;
    }

    return start_addr;
/* -------------------------------------------------------- PROJECT3 : Virtual Memory - Memory Mapped Files -------------------------------------------------------- */
}

/* Do the munmap */
/* 매핑을 해제하는 함수 */
void
do_munmap (void *addr) {
/* -------------------------------------------------------- PROJECT3 : Virtual Memory - Memory Mapped Files -------------------------------------------------------- */
	struct supplemental_page_table *spt = &thread_current ()->spt;
    struct page *p = spt_find_page (spt, addr);
    int count = p->mapped_page_count; // 같은 파일이 매핑된 페이지가 모두 해제될 수 있도록, 매핑할 때 저장해 둔 매핑된 페이지 수를 이용
    for (int i = 0; i < count; i++) {
        if (p)
            destroy (p); // destroy 매크로에 연결되어 있는 file_backed_destroy() 함수에서 파일의 수정사항을 반영하고 가상 페이지 목록에서 페이지를 제거
            
        addr += PGSIZE;
        p = spt_find_page (spt, addr);
    }
/* -------------------------------------------------------- PROJECT3 : Virtual Memory - Memory Mapped Files -------------------------------------------------------- */
}