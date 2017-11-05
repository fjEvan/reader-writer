// 20171103  Jiao Feng  here's to fairness

/* Thought process:
   Readers are favored with the original solution.
   Since each thread will do the same number of operations as specified,
   take one round as all the reader and writer threads 
   each doing one operation.
   To make it fair, try to make the number of operations every round
   the same as the number of threads that reader / writer has.
                NO_of_R_Thread     NO_Of_R_Operation
   Namely, let ---------------- = -------------------  each round.
                NO_of_W_Thread     NO_of_W_Operation

   Then, at any point after some operations, the progress for both 
   reader and writer would be even.
*/

#include "semaphore.h"

/*
 * This class provides the reader and writer lock controls using semaphores
 * as described in the text.
 */
class readerwriter {
public:
	readerwriter(): m_readcount(0), m_writesem(1), m_mutex(1), m_writerVIP(1), 
	                m_getInLine(1), m_writecount(0), m_mutexW(1) { }
	/*
	 * Operations to synchronize readers and writers.
	 */
	void enter_read()
	{
		m_getInLine.down();
		m_getInLine.up();

		m_writerVIP.down(); // blocks until writer threads catch up
		m_mutex.down();
		if(++m_readcount == 1) m_writesem.down();
		m_mutex.up();
		m_writerVIP.up();
	}

	void leave_read()
	{	
		m_mutex.down();
		if(--m_readcount == 0)  m_writesem.up();
		m_mutex.up();
	}
	

	void enter_write(int numberOfWriters)  // pass a parameter
	{    // get the number of writer threads from command or by default
		m_getInLine.down();  
	  // suspend potential readers as soon as a writer wants to write
		m_mutexW.down();    // protect the update of a counter
		
		if(m_writecount == 0)  
		{
			m_writecount = numberOfWriters; // run that number of operations
			m_writerVIP.down();  // keep blocking new readers until satisfied
		}

		m_mutexW.up();				
		m_writesem.down();
	}

	void leave_write()
	{
		m_writesem.up();
		m_mutexW.down();

		if(--m_writecount == 0)  m_writerVIP.up();  // readers ok to go
		m_mutexW.up();
		m_getInLine.up();				
	}

private:
	int m_readcount;
	int m_writecount;
	semaphore m_mutex;
	semaphore m_mutexW;
	semaphore m_writesem;
	semaphore m_writerVIP;
	semaphore m_getInLine;
};



