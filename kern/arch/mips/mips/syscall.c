#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <machine/pcb.h>
#include <machine/spl.h>
#include <machine/trapframe.h>
#include <kern/callno.h>
#include <syscall.h>

// My includes
#include <curthread.h>
#include <addrspace.h>
#include <thread.h>
#include <synch.h>
#include <vnode.h>
#include <uio.h>
#include "opt-synchprobs.h"
#include <kern/unistd.h>
#include <vfs.h>
#include <machine/spl.h>
#include <kern/limits.h>
#include <test.h>
#include <vm.h>
// My includes --END--

/*
 * System call handler.
 *
 * A pointer to the trapframe created during exception entry (in
 * exception.S) is passed in.
 *
 * The calling conventions for syscalls are as follows: Like ordinary
 * function calls, the first 4 32-bit arguments are passed in the 4
 * argument registers a0-a3. In addition, the system call number is
 * passed in the v0 register.
 *
 * On successful return, the return value is passed back in the v0
 * register, like an ordinary function call, and the a3 register is
 * also set to 0 to indicate success.
 *
 * On an error return, the error code is passed back in the v0
 * register, and the a3 register is set to 1 to indicate failure.
 * (Userlevel code takes care of storing the error code in errno and
 * returning the value -1 from the actual userlevel syscall function.
 * See src/lib/libc/syscalls.S and related files.)
 *
 * Upon syscall return the program counter stored in the trapframe
 * must be incremented by one instruction; otherwise the exception
 * return code will restart the "syscall" instruction and the system
 * call will repeat forever.
 *
 * Since none of the OS/161 system calls have more than 4 arguments,
 * there should be no need to fetch additional arguments from the
 * user-level stack.
 *
 * Watch out: if you make system calls that have 64-bit quantities as
 * arguments, they will get passed in pairs of registers, and not
 * necessarily in the way you expect. We recommend you don't do it.
 * (In fact, we recommend you don't use 64-bit quantities at all. See
 * arch/mips/include/types.h.)
 */

void
mips_syscall(struct trapframe *tf)
{
	int callno;
	int32_t retval;
	int err;
	int byte;
	//kprintf("in syscall and callno is %d\n",tf->tf_v0);
	assert(curspl==0);

	callno = tf->tf_v0;
	//if (callno!=6&&callno!=5)
	//kprintf("finally sbrk");
	/*
	 * Initialize retval to 0. Many of the system calls don't
	 * really return a value, just 0 for success and -1 on
	 * error. Since retval is the value returned on success,
	 * initialize it to 0 by default; thus it's not necessary to
	 * deal with it except for calls that return other values, 
	 * like write.
	 */

	retval = 0;

	switch (callno) {
	    case SYS_reboot:
		err = sys_reboot(tf->tf_a0);
		break;
      case SYS_fork:
    retval = sys_fork(tf);
    if (retval == ENOMEM) {
      //kprintf("forking right now and I am out of memory\n");
      err = ENOMEM;
    }
    else err = 0;
    break;
      case SYS_waitpid:
    retval  = sys_waitpid(tf->tf_a0, tf->tf_a1, tf->tf_a2);
    err = 0;
    break;
      case SYS__exit:
    sys__exit(tf->tf_a0);
    err = 0;
    break;
      case SYS_getpid:
    retval = sys_getpid(&retval);
    err = 0;
    break;
      case SYS_write:
    err = sys_write(tf->tf_a0, (void *) tf->tf_a1, tf->tf_a2, &retval);
    break;
      case SYS_read:
    err = sys_read(tf->tf_a0, (void *) tf->tf_a1, tf->tf_a2, &retval);
    break;
      case SYS_execv:
    retval = sys_execv((char *)tf->tf_a0,(char **)tf->tf_a1);
    err = 0;
    break;
      case SYS_sbrk:
	//kprintf("im in SYS_sbrk\n");
	byte = (int)tf->tf_a0;
	if(byte>= 0) //means not negative
	{
    	retval = sys_sbrk(byte);
    	err = 0;
	}
	else 
	{
        retval = EINVAL;
	err = 0;
        }
    break;

	    /* Add stuff here */
 
	    default:
		kprintf("Unknown syscall %d\n", callno);
		err = ENOSYS;
		break;
	}

  

	if (err) {
		/*
		 * Return the error code. This gets converted at
		 * userlevel to a return value of -1 and the error
		 * code in errno.
		 */
		tf->tf_v0 = err;
		tf->tf_a3 = 1;      /* signal an error */
	}
	else {
		/* Success. */
		tf->tf_v0 = retval;
		tf->tf_a3 = 0;      /* signal no error */
	}
	
	/*
	 * Now, advance the program counter, to avoid restarting
	 * the syscall over and over again.
	 */
	
	tf->tf_epc += 4;

	/* Make sure the syscall code didn't forget to lower spl */
	//assert(curspl==0);
}

void
md_forkentry(struct trapframe *tf, unsigned long as_data)
{
	/*
	 * This function is provided as a reminder. You need to write
	 * both it and the code that calls it.
	 *
	 * Thus, you can trash it and do things another way if you prefer.
	 */

  struct trapframe tf_local;
  struct trapframe* newtf;
  struct addrspace* newas = (struct addrspace*) as_data;

  tf_local = *tf;
  newtf = &tf_local;

  newtf->tf_v0 = 0;
  newtf->tf_a3 = 0;
  newtf->tf_epc += 4;

  curthread->t_vmspace = newas;

  as_activate(curthread->t_vmspace);

  mips_usermode(&tf_local);
}

int sys_fork(struct trapframe* tf/*, int* retval*/) {
  //kprintf("fork is getting called\n");


  struct thread* newthread;
  struct trapframe* newtf;
  struct addrspace* newas;
  //struct trapframe tf_local;
  int result;

  newtf = (struct trapframe*) kmalloc(sizeof(struct trapframe));
  if (newtf == NULL) {
    //*retval = ENOMEM;
    return ENOMEM;
  }
  *newtf = *tf;

  int ret = as_copy(curthread->t_vmspace, &newas);
  if (newas == NULL || ret != 0) {
    kfree(newtf);
    //*retval = ENOMEM;
    return ENOMEM;
  }

  as_activate(curthread->t_vmspace);

  result = thread_fork(curthread->t_name, newtf,(unsigned long)newas,(void (*)(void *, unsigned long)) md_forkentry,&newthread);

  if (result) {
    kfree(newtf);
    return ENOMEM;
  }
  
  //newthread->ppid = curthread->pid;
  //newthread->pid = add_pid();

  tf->tf_v0 = newthread->pid;

  return newthread->pid;
}

int sys__exit (int exitcode) {
  struct pid_list* ptr = curthread->ptr;
  ptr->status = THREAD_EXITED;
  ptr->exitcode = exitcode;
  thread_exit();
  return 0;
}

int sys_getpid (int* retval) {
  *retval = (int)curthread->pid;
  return 0;
}

int sys_waitpid (pid_t pid, int* status, int options) {
  struct pid_list* child = NULL;
  child = get_process(pid);
  if (child == NULL) return EINVAL;
  if (options != 0) return EINVAL;
  if (status == NULL) return EINVAL;
  lock_acquire(pid_list_lock);
  if (pid_list_head == NULL) panic("sys_waitpid: head list does not exists\n");
  if (child->ppid != curthread->pid) {
    lock_release(pid_list_lock);
    return EINVAL;
  }
  if (child->status == THREAD_EXITED) {
    *status = child->exitcode;
    //kprintf("This is *status: %d 0x%lx\n",*status, status);
    //if (*status != 0) kprintf("returning with a value greater than zero.\n");
  } else {
    cv_wait(child->wait_cv,pid_list_lock);
    *status = child->exitcode;
    //kprintf("This is *status: %d 0x%lx\n",*status, status);
    //if (*status != 0) kprintf("returning with a value greater than zero.\n");
  }
  lock_release(pid_list_lock);
  return 0;
}

int
sys_execv(char *progname, char **args)
{

kprintf("im in execv\n");
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result;
	int argc,spl;
	int i = 0;
	size_t bufferlen;// not sure y this is nessisary but is needed

	kprintf("am i in exec ???\n");
	spl=splhigh();
	
	char *prog = (char*)kmalloc(PATH_MAX); //copy the name of prog to the kernel
	if(prog == NULL)
	{
		return ENOMEM;
	}
 	//strcpy(prog,progname);
	copyinstr((userptr_t)progname,prog,PATH_MAX,&bufferlen);

	while (args[i]!=NULL)
	{
	i++;
	}
	
	argc = i;
	char **argv;
	argv = (char **)kmalloc(sizeof(char*));
	if(argv == NULL) {
		kfree(prog);
		return ENOMEM;
	}

	int j,k;
	
	for (i=0; i<=argc; i++)
	{
	if(i<argc)
		{
		int length;
		length = strlen(args[i]);
		length=length+1; ///NULL
		argv[i]=(char*)kmalloc(length+1);
		if(argv[i]==NULL) // run outa mem
			{
				for(j = 0; j < i; j++) //free all the ones allocated b4 as well
				{
				kfree(argv[j]);
				}
				kfree(argv);
				kfree(prog);
				return ENOMEM;
			}
		copyinstr((userptr_t)args[i], argv[i], length, &bufferlen);
		kprintf("agrv is %s, i is %d\n", argv[i],i);
		}
	else
	argv[i] = NULL;
	}

	kprintf("b4 open ???\n");
	//Open the file. 
	kprintf("arg1 and 2 are %s,%s\n",argv[1],argv[2]);
	result = vfs_open(prog, O_RDONLY, &v);
	if (result) {
		return result;
	}



	if (curthread->t_vmspace) 
	{
	struct addrspace *as = curthread->t_vmspace;
	curthread->t_vmspace = NULL;
	as_destroy(as);
	}
	/*//destroy old vm
	if(curthread->t_vmspace != NULL) {
		as_destroy(curthread);
		curthread->t_vmspace = NULL;
		curthread->t_cwd = NULL;
	}*/
	// We should be a new thread. 
	assert(curthread->t_vmspace == NULL);


      // Create a new address space. 
	curthread->t_vmspace = as_create();
	if (curthread->t_vmspace==NULL) {
		vfs_close(v);
		return ENOMEM;
	}

	// Activate it. 
	as_activate(curthread->t_vmspace);
	//kprintf("1\n");
	//Load the executable. 
	result = load_elf(v, &entrypoint);
	//kprintf("2\n");
	if (result) {
		// thread_exit destroys curthread->t_vmspace /
		vfs_close(v);
		return result;
	}
	//kprintf("3?\n");
	// Done with the file now. /
	vfs_close(v);

	//Define the user stack in the address space /
	result = as_define_stack(curthread->t_vmspace, &stackptr);
	//kprintf("4\n");
	if (result) {
		// thread_exit destroys curthread->t_vmspace /
		return result;
	}
	
	//unsigned int storedstack[argc];
	int argvptr[argc];
	//int o;
	int totaloffset=0;
	for(i = argc-1; i >= 0; i--) {
		int length = strlen(argv[i])+1;
		if(length%4 != 0)
		{
		int remainder = (length%4);
		totaloffset=(length + (4-remainder));
		stackptr = stackptr - totaloffset;
		}
		else
		{
		stackptr = stackptr - length;
		}
		copyoutstr(argv[i], (userptr_t)stackptr, length, &bufferlen);
		kprintf("copyout argv is %s and i is %d\n",argv[i], i);
		argvptr[i] = stackptr;
	}
	argvptr[argc] = 0;
	
	for(i = argc; i >= 0; i--)
	{
		stackptr = stackptr-4;
		
		copyout(&argvptr[i] ,(userptr_t)stackptr, sizeof(argvptr[i]));
	}
	


	kfree(argv);
	kfree(prog);

	// Warp to user mode. 
	splx(spl);

	//md_usermode(0 , NULL ,
	//	    stackptr, entrypoint);

	md_usermode(argc , (userptr_t)stackptr, stackptr, entrypoint);
	
	// md_usermode does not return 
	panic("md_usermode returned\n");

	return EINVAL;

}

int sys_write(int fd, /*userptr_t buf*/ char* c, size_t size, int* retval) {
  (void*) fd;
  (void*) size;
  char buf[2];
  buf[0] = *c;
  buf[1] = '\0';
  //if (buf[1] == 99) kprintf("there is no 99\n");
  kprintf("%s",buf);
  *retval = 0;
  return 0;
}

int sys_read(int fd, /*userptr_t buf*/ char* buf, size_t size, int* retval) {
  char c;
  c = getch();
  *buf=c;
  size=sizeof(char);
  *retval = (int) c;
  return 0;
}

void remove_all_exited_children (pid_t pid) {
  lock_acquire(pid_list_lock);
  if (pid_list_head == NULL) {
    lock_release(pid_list_lock);
    return;
  }
  struct pid_list* curr = pid_list_head;
  struct pid_list* prev = NULL;
  struct pid_list* to_delete;

  while (curr != NULL) {
    if (curr->ppid == pid && curr->status == THREAD_EXITED) {
      to_delete = curr;
      if (prev == NULL) {
        curr = curr->next;
        pid_list_head = pid_list_head->next;
        cv_destroy(to_delete->wait_cv);
        kfree(to_delete);
      } else {
        curr = curr->next;
        prev->next = curr;
        cv_destroy(to_delete->wait_cv);
        kfree(to_delete);
      }
      pid_list_tail = pid_list_head;
      while (pid_list_tail->next != NULL) pid_list_tail = pid_list_tail->next;
    }
    if (curr == NULL) break;
    prev = curr;
    curr = curr->next;
  }
  lock_release(pid_list_lock);
}

struct pid_list* get_process (pid_t pid) {
  if (pid_list_head == NULL) {
    kprintf("error: pid_list_head is NULL\n");
    return NULL;
  }
  struct pid_list* traverser = pid_list_head;
  while (traverser != NULL) {
    if (traverser->pid == pid) {
      return traverser;
    }
    traverser = traverser->next;
  }
  return NULL;
}

int sys_sbrk(int byte) {
  int spl = splhigh();
  assert(curthread->t_vmspace != NULL);
  if (byte < 0) {
    return -1;
  }
  struct addrspace *as;
  as=curthread->t_vmspace;
  u_int32_t oldheap, newheap;
  oldheap= as->heapend;
  newheap= as->heapend + (unsigned)byte;
  if ((newheap - as->heapstart)>0x500000) {
    return -1;
  }
  as->heapend=newheap;
  splx(spl);
  return oldheap;
}
