#include <syscall.h>


void _halt(void)
{
	clear_console();        
    printf("Shutting down...");
    sim_halt();

    // TODO power off
    while(1);
}

int _readfile(char *filename, char *buf, int count, int offset)
{
	// verify filename ,buf
	if (count < 0 || offset < 0)
	{
	return -1;
	}
	int size = 0;
	current_thread -> state = THREAD_NONSCHEDULABLE;
	size = getbytes(filename, offset, count, buf);
	current_thread -> state = THREAD_RUNNING;
	return size;

}
