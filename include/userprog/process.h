#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_create_initd (const char *file_name);
tid_t process_fork (const char *name, struct intr_frame *if_);
int process_exec (void *f_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (struct thread *next);

/* -------------------------------------------------------- PROJECT2 : User Program - Argument Passing -------------------------------------------------------- */
void argument_stack (char **argv, int argc, struct intr_frame *_if); // 파싱한 인자를 유저 스택에 넣어주는 함수 선언
/* -------------------------------------------------------- PROJECT2 : User Program - Argument Passing -------------------------------------------------------- */

/* -------------------------------------------------------- PROJECT2 : User Program - System Call -------------------------------------------------------- */
struct thread *get_child_process (int pid); // 자식이 로드될 때까지 대기하기 위해서 방금 생성한 자식 스레드를 자식 리스트에서 검색하는 함수 선언
/* -------------------------------------------------------- PROJECT2 : User Program - System Call -------------------------------------------------------- */

/* -------------------------------------------------------- PROJECT3 : Virtual Memory - Memory Mapped Files -------------------------------------------------------- */
bool lazy_load_segment (struct page *page, void *aux);

/* 내용을 로드할 때 vm_alloc_page_with_initializer() 함수로 넘겨줘야 할 인자를 가지고 있는 구조체 */
struct lazy_load_arg {
	struct file *file; // 로드할 파일의 내용이 담긴 파일 객체
	off_t ofs; // upage에서 읽기 시작할 위치
	uint32_t read_bytes; // upage에서 읽어야 하는 바이트 수
	uint32_t zero_bytes; // upage에서 read_bytes만큼 읽고 공간이 남아 0으로 채워야 하는 바이트 수
};
/* -------------------------------------------------------- PROJECT3 : Virtual Memory - Memory Mapped Files -------------------------------------------------------- */
#endif /* userprog/process.h */
