/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"

/* -------------------------------------------------------------- PROJECT3 : Virtual Memory - Frame Table -------------------------------------------------------------- */
#include "include/threads/mmu.h" // pml4_set_page() 함수를 사용하기 위해 헤더 추가
/* -------------------------------------------------------------- PROJECT3 : Virtual Memory - Frame Table -------------------------------------------------------------- */

/* ------------------------------------------------------------ PROJECT3 : Virtual Memory - Anonymous Page ------------------------------------------------------------ */
#include "include/threads/thread.h"
/* ------------------------------------------------------------ PROJECT3 : Virtual Memory - Anonymous Page ------------------------------------------------------------ */

/* -------------------------------------------------------- PROJECT3 : Virtual Memory - Memory Mapped Files -------------------------------------------------------- */
#include "include/userprog/process.h"
/* -------------------------------------------------------- PROJECT3 : Virtual Memory - Memory Mapped Files -------------------------------------------------------- */

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
/* -------------------------------------------------------- PROJECT3 : Virtual Memory - Swap In/Out -------------------------------------------------------- */
	list_init(&frame_table);
	lock_init(&frame_table_lock);
/* -------------------------------------------------------- PROJECT3 : Virtual Memory - Swap In/Out -------------------------------------------------------- */
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* -------------------------------------------------------- PROJECT3 : Virtual Memory - Supplemental Page Table -------------------------------------------------------- */
unsigned page_hash (const struct hash_elem *p_, void *aux UNUSED); // 보조 데이터 AUX를 사용하여 해시 테이블 H를 초기화하여 해시 값을 계산하는 함수 선언
bool page_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED); // 해시 요소를 비교하는 함수 선언
/* -------------------------------------------------------- PROJECT3 : Virtual Memory - Supplemental Page Table -------------------------------------------------------- */

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
/* 전달된 vm_type에 따라 적합한 초기화 함수를 가져와서 uninit_new() 함수를 호출하는 함수 */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

/* ------------------------------------------------------------ PROJECT3 : Virtual Memory - Anonymous Page ------------------------------------------------------------ */
	if (spt_find_page (spt, upage) == NULL) { // upage가 이미 사용 중인지 확인
		struct page *p = (struct page *) malloc (sizeof (struct page)); // 페이지 구조체를 할당
		bool (*page_initializer) (struct page *, enum vm_type, void *); // type에 따른 초기화 함수를 가져옴

		// VM_TYPE에 따라 서로 다른 initializer를 함수 포인터로 전달
		// anon 타입이면 anon_initializer를, file_backed 타입이면 file_backed_initializer
		switch (VM_TYPE (type)) {
			case VM_ANON:
				page_initializer = anon_initializer;
				break;
			case VM_FILE:
				page_initializer = file_backed_initializer;
				break;
		}

		uninit_new (p, upage, init, type, aux, page_initializer); // uninit 타입의 페이지로 초기화
		p->writable = writable; // 매개변수 writable을 페이지 구조체의 writable 필드에 할당

		return spt_insert_page (spt, p); // 생성한 페이지를 supplemental page table에 추가
	}
/* ------------------------------------------------------------ PROJECT3 : Virtual Memory - Anonymous Page ------------------------------------------------------------ */
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
/* supplementary page table에서 va에 해당하는 구조체 페이지를 찾아 반환하는 함수(실패할 경우 NULL 반환) */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	struct page *page = NULL; // 페이지 구조체 포인터 변수 초기화
/* -------------------------------------------------------- PROJECT3 : Virtual Memory - Supplemental Page Table -------------------------------------------------------- */
	page = (struct page *) malloc (sizeof (struct page)); // dummy page 생성
	struct hash_elem *e;

	page->va = pg_round_down (va); // va가 가리키는 가상 페이지의 시작 포인트 반환
	e = hash_find (&spt->spt_hash, &page->hash_elem); // hash에서 hash_elem과 같은 요소를 검색해서 발견한 element가 있으면 element를, 아니면 NULL 반환

	free (page); // dummy page 해제

	return e != NULL ? hash_entry (e, struct page, hash_elem) : NULL; // e가 NULL이 아니면 hash_entry() 함수로 hash_elem이 포함된 페이지를 찾아 반환하고, 아니면 NULL 반환
/* -------------------------------------------------------- PROJECT3 : Virtual Memory - Supplemental Page Table -------------------------------------------------------- */
}

/* Insert PAGE into spt with validation. */
/* 페이지를 유효성 검사를 수행하여 spt에 삽입하는 함수 */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
/* -------------------------------------------------------- PROJECT3 : Virtual Memory - Supplemental Page Table -------------------------------------------------------- */
	return hash_insert (&spt->spt_hash, &page->hash_elem) == NULL ? true : false; // hash_insert() 함수 반환값이 NULL이면 true, 아니면 false 반환
/* -------------------------------------------------------- PROJECT3 : Virtual Memory - Supplemental Page Table -------------------------------------------------------- */
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	hash_delete(&spt->spt_hash, &page->hash_elem);
	vm_dealloc_page (page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: The policy for eviction is up to you. */
/* -------------------------------------------------------- PROJECT3 : Virtual Memory - Swap In/Out -------------------------------------------------------- */
	struct thread *curr = thread_current();

	lock_acquire(&frame_table_lock);
	struct list_elem *start = list_begin(&frame_table);
	for (start; start != list_end(&frame_table); start = list_next(start))
	{
		victim = list_entry(start, struct frame, frame_elem);
		if (victim->page == NULL) // frame에 할당된 페이지가 없는 경우 (page가 destroy된 경우 )
		{
			lock_release(&frame_table_lock);
			return victim;
		}
		if (pml4_is_accessed(curr->pml4, victim->page->va))
			pml4_set_accessed(curr->pml4, victim->page->va, 0);
		else
		{
			lock_release(&frame_table_lock);
			return victim;
		}
	}
	lock_release(&frame_table_lock);
/* -------------------------------------------------------- PROJECT3 : Virtual Memory - Swap In/Out -------------------------------------------------------- */
	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	/* TODO: swap out the victim and return the evicted frame. */
/* -------------------------------------------------------- PROJECT3 : Virtual Memory - Swap In/Out -------------------------------------------------------- */
	struct frame *victim = vm_get_victim();

	if (victim->page)
		swap_out(victim->page);

	return victim;
/* -------------------------------------------------------- PROJECT3 : Virtual Memory - Swap In/Out -------------------------------------------------------- */
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
/* palloc()을 사용하여 프레임을 얻습니다.
   사용 가능한 페이지가 없는 경우 페이지를 대체하여 사용 가능한 메모리 공간을 확보합니다.
   이 함수는 항상 유효한 주소를 반환합니다.
   즉, 사용자 풀 메모리가 가득 찬 경우에도 이 함수는 프레임을 대체하여 사용 가능한 메모리 공간을 얻습니다. */
/* user pool에서 새로운 물리 페이지를 가져와서 새로운 frame 구조체에 할당해서 반환하는 함수 */
static struct frame *
vm_get_frame (void) {
	struct frame *frame = NULL; // 프레임 구조체 포인터 변수 초기화
/* -------------------------------------------------------------- PROJECT3 : Virtual Memory - Frame Table -------------------------------------------------------------- */
	void *kva = palloc_get_page (PAL_USER); // 물리 페이지를 할당하고 해당 페이지의 커널 가상 주소를 반환하는 palloc_get_page() 함수를 호출하여 사용자 풀에서 새로운 물리적 페이지 확득

	// 페이지 할당에 실패한 경우(palloc_get_page() 함수에서 사용 가능한 페이지가 없어 NULL 포인터가 반환된 경우) 패닉 메시지 출력
	// if (kva == NULL)
	// 	PANIC ("todo");
	if (kva == NULL) // page 할당 실패
	{
		struct frame *victim = vm_evict_frame();
		victim->page = NULL;
		return victim;
	}

	// 페이지 할당에 성공한 경우, 프레임을 할당하고 멤버 초기화
	frame = (struct frame *) malloc (sizeof (struct frame));
	frame->kva = kva;
	frame->page = NULL;

	lock_acquire(&frame_table_lock);
	list_push_back(&frame_table, &frame->frame_elem);
	lock_release(&frame_table_lock);
/* -------------------------------------------------------------- PROJECT3 : Virtual Memory - Frame Table -------------------------------------------------------------- */

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	return frame;
}

/* Growing the stack. */
/* Stack growth 함수 */
static void
vm_stack_growth (void *addr UNUSED) {
/* ------------------------------------------------------------ PROJECT3 : Virtual Memory - Stack Growth ------------------------------------------------------------ */
	vm_alloc_page(VM_ANON | VM_MARKER_0, pg_round_down(addr), 1);
/* ------------------------------------------------------------ PROJECT3 : Virtual Memory - Stack Growth ------------------------------------------------------------ */
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
/* page fault가 발생하면 제어권을 전달 받는 함수 */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;
/* ------------------------------------------------------------ PROJECT3 : Virtual Memory - Anonymous Page ------------------------------------------------------------ */
	// 인자로 받은 주소가 NULL인 경우 false 반환
	if (addr == NULL)
		return false;
	// 인자로 받은 주소가 커널 영역에 있는 주소인 경우 false 반환
	if (is_kernel_vaddr (addr))
        return false;
	// 접근한 메모리의 물리 페이지가 존재하지 않는 경우
	if (not_present) {
/* ------------------------------------------------------------ PROJECT3 : Virtual Memory - Stack Growth ------------------------------------------------------------ */
		// 페이지 폴트가 스택 확장에 대한 유효한 경우인지 확인
		void *rsp = f->rsp; // user access인 경우 유저 스택을 가져옴
		// kernel access인 경우 스레드에서 유저 스택을 가져옴
		if (!user) 
			rsp = thread_current ()->rsp;

		// 스택 확장으로 처리할 수 있는 페이지 폴트인 경우, vm_stack_growth()함수 호출
		//////////////////////////////////////////////////// 변경 ////////////////////////////////////////////////////
		// if (addr <= USER_STACK && addr > USER_STACK - (1 << 20)) {
        //     if (rsp - 8 > USER_STACK - (1 << 20) && addr == rsp - 8)
        //         vm_stack_growth(addr);
        //     else if (rsp > USER_STACK - (1 << 20) && addr >= rsp)
        //         vm_stack_growth(addr);
        // }
		if ((USER_STACK - (1 << 20) <= rsp - 8 && rsp - 8 == addr && addr <= USER_STACK) || (USER_STACK - (1 << 20) <= rsp && rsp <= addr && addr <= USER_STACK))
			vm_stack_growth(addr);
/* ------------------------------------------------------------ PROJECT3 : Virtual Memory - Stack Growth ------------------------------------------------------------ */
		page = spt_find_page (spt, addr); // supplemental page table에서 해당 주소에 해당하는 페이지가 있는지 확인
		// 페이지가 존재하지 않는 경우 false 반환
		if (page == NULL)
			return false;
		 // write 불가능한 페이지에 write 요청한 경우 false 반환
		if (write == 1 && page->writable == 0)
			return false;
		// 페이지가 존재하는 경우 vm_do_claim_page() 함수를 호출
		return vm_do_claim_page (page);
	}
	// not_present 값이 false인 경우 false 반환
	// 물리 프레임이 할당되어 있지만, page fault가 일어난 경우로 read only page에 write한 경우이므로 예외 처리 필요
	return false;
/* ------------------------------------------------------------ PROJECT3 : Virtual Memory - Anonymous Page ------------------------------------------------------------ */
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
/* Supplemental page table에서 va에 해당하는 페이지를 가져와 프레임과 매핑을 요청하는 함수 */
bool
vm_claim_page (void *va UNUSED) {
	struct page *page = NULL; // 페이지 구조체 초기화
/* -------------------------------------------------------------- PROJECT3 : Virtual Memory - Frame Table -------------------------------------------------------------- */
	page = spt_find_page (&thread_current ()->spt, va); // spt_find_page() 함수를 호출하여 va에 해당하는 페이지 획득

	// va에 해당하는 페이지가 없을 경우(=NULL) false 반환
	if (page == NULL)
		return false;

	return vm_do_claim_page (page); // va에 해당하는 페이지를 가져오는데 성공한 경우, 페이지와 프레임을 매핑하는 vm_do_claim_page() 함수 호출 
/* -------------------------------------------------------------- PROJECT3 : Virtual Memory - Frame Table -------------------------------------------------------------- */
}

/* Claim the PAGE and set up the mmu. */
/* PAGE를 요청하고 MMU를 설정합니다. */
/* 새 프레임을 가져와서 페이지와 매핑하는 함수 */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame (); // vm_get_frame()를 호출하여 프레임 획득

	/* Set links */
	// 페이지와 프레임 링크 설정
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
/* -------------------------------------------------------------- PROJECT3 : Virtual Memory - Frame Table -------------------------------------------------------------- */
	struct thread *current = thread_current (); // 현재 실행중인 스레드 구조체 저장
	pml4_set_page (current->pml4, page->va, frame->kva, page->writable); // pml4_set_page() 함수를 호출하여 프로세스의 pml4에 페이지 가상 주소와 프레임 물리 주소를 서로 매핑한 결과 저장
/* -------------------------------------------------------------- PROJECT3 : Virtual Memory - Frame Table -------------------------------------------------------------- */

	return swap_in (page, frame->kva);
}

/* Initialize new supplemental page table */
/* supplemental page table을 초기화하는 함수 */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
/* -------------------------------------------------------- PROJECT3 : Virtual Memory - Supplemental Page Table -------------------------------------------------------- */
	hash_init (&spt->spt_hash, page_hash, page_less, NULL); // 해시 테이블의 해시를 초기화하는 hash_init() 함수 호출하여 supplemental page table을 초기화
/* -------------------------------------------------------- PROJECT3 : Virtual Memory - Supplemental Page Table -------------------------------------------------------- */
}

/* Copy supplemental page table from src to dst */
/* 보충 페이지 테이블을 src에서 dst로 복사합니다. */
/* 자식 프로세스를 생성할 때 부모 프로세스의 supplemental page table을 자식에서 복사하는 함수 */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
	// TODO: 보조 페이지 테이블을 src에서 dst로 복사합니다.
	// TODO: src의 각 페이지를 순회하고 dst에 해당 entry의 사본을 만듭니다.
	// TODO: uninit page를 할당하고 그것을 즉시 claim해야 합니다.
/* ------------------------------------------------------------ PROJECT3 : Virtual Memory - Anonymous Page ------------------------------------------------------------ */
	struct hash_iterator i;
	hash_first (&i, &src->spt_hash);
	while (hash_next (&i))
	{
		// src_page 정보
		struct page *src_page = hash_entry (hash_cur (&i), struct page, hash_elem);
		enum vm_type type = src_page->operations->type;
		void *upage = src_page->va;
		bool writable = src_page->writable;

		// type이 VM_UNINIT인 경우
		if (type == VM_UNINIT) {
			// UNINIT 페이지 생성 및 초기화
			vm_initializer *init = src_page->uninit.init;
			void *aux = src_page->uninit.aux;
			vm_alloc_page_with_initializer (VM_ANON, upage, writable, init, aux);
			continue;
		}

/* -------------------------------------------------------- PROJECT3 : Virtual Memory - Memory Mapped Files -------------------------------------------------------- */
		// type이 VM_FILE인 경우
        if (type == VM_FILE) {
            struct lazy_load_arg *file_aux = malloc(sizeof(struct lazy_load_arg));
            file_aux->file = src_page->file.file;
            file_aux->ofs = src_page->file.ofs;
            file_aux->read_bytes = src_page->file.read_bytes;
            file_aux->zero_bytes = src_page->file.zero_bytes;
            if (!vm_alloc_page_with_initializer(type, upage, writable, NULL, file_aux))
                return false;
            struct page *file_page = spt_find_page(dst, upage);
            file_backed_initializer(file_page, type, NULL);
            file_page->frame = src_page->frame;
            pml4_set_page(thread_current()->pml4, file_page->va, src_page->frame->kva, src_page->writable);
            continue;
        }
/* -------------------------------------------------------- PROJECT3 : Virtual Memory - Memory Mapped Files -------------------------------------------------------- */

		// type이 VM_UNINIT이 아닌 경우 false 반환
		if (!vm_alloc_page(type, upage, writable))
			return false;

		// vm_claim_page() 함수를 호출하여 매핑 및 페이지 타입에 맞게 초기화하고, 실패한 경우 false 반환
		if (!vm_claim_page (upage))
			return false;

		// 매핑된 프레임에 내용 로딩
		struct page *dst_page = spt_find_page (dst, upage);
		memcpy (dst_page->frame->kva, src_page->frame->kva, PGSIZE);
	}
	return true;

/* ------------------------------------------------------------ PROJECT3 : Virtual Memory - Anonymous Page ------------------------------------------------------------ */
}

/* ------------------------------------------------------------ PROJECT3 : Virtual Memory - Anonymous Page ------------------------------------------------------------ */
/* 해시 페이지를 가져와 페이지 타입에 따른 destroy() 함수를 호출하여 해당 페이지의 리소스를 해제하는 함수 */
void
hash_page_destroy (struct hash_elem *e, void *aux) {
	struct page *page = hash_entry (e, struct page, hash_elem); // hash element에 해당하는 페이지를 가져옴
	destroy (page); // 페이지 타입에 따른 destroy() 함수를 호출
	free (page); // 페이지 리소스 해제
}
/* ------------------------------------------------------------ PROJECT3 : Virtual Memory - Anonymous Page ------------------------------------------------------------ */

/* Free the resource hold by the supplemental page table */
/* supplemental page table의 리소스를 해제하는 함수 */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
/* ------------------------------------------------------------ PROJECT3 : Virtual Memory - Anonymous Page ------------------------------------------------------------ */
	/* TODO: Destroy all the supplemental_page_table hold by thread and
     * TODO: writeback all the modified contents to the storage. */
	hash_clear (&spt->spt_hash, hash_page_destroy); // 해시 테이블의 모든 요소를 제거
/* ------------------------------------------------------------ PROJECT3 : Virtual Memory - Anonymous Page ------------------------------------------------------------ */
}

/* -------------------------------------------------------- PROJECT3 : Virtual Memory - Supplemental Page Table -------------------------------------------------------- */
/* 보조 데이터 AUX를 사용하여 해시 테이블 H를 초기화하여 해시 값을 계산하는 함수 */
unsigned
page_hash (const struct hash_elem *p_, void *aux UNUSED) {
	const struct page *p = hash_entry (p_, struct page, hash_elem); // 페이지 p에 들어있는 해시 테이블 시작 주소 획득
	return hash_bytes (&p->va, sizeof p->va); // hash_bytes() 함수를 호출하여 페이지 p의 가상 주소에 대한 해시 값 반환
}

/* 해시 요소를 비교하는 함수 */
bool
page_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED) {
	const struct page *a = hash_entry (a_, struct page, hash_elem); // 페이지 a에 들어있는 해시 테이블 시작 주소 획득
	const struct page *b = hash_entry (b_, struct page, hash_elem); // 페이지 b에 들어있는 해시 테이블 시작 주소 획득
	return a->va < b->va; // 첫 번째 페이지의 가상 주소가 두 번째 페이지의 가상 주소보다 작으면 true, 크면 false 반환
}
/* -------------------------------------------------------- PROJECT3 : Virtual Memory - Supplemental Page Table -------------------------------------------------------- */
