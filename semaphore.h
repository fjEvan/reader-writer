#pragma once
#ifndef __semaphore_h__
#define __semaphore_h__

#include <mutex>
#include <condition_variable>

/*
 * C++ doesn't actually have semaphores.  It has mutexes and condition variables,
 * which we can use to make a semaphore.
 */

class semaphore {
public:
	semaphore(int init): m_value(init) { }

	/* Semaphore down operation. */
	void down();

	/* Semaphore up operation. */
	void up();
private:
	int m_value;			// Value of semaphore.
	std::mutex m_mux;		// Controls access.
	std::condition_variable m_waitcond; // Controls waiting and restart
};

#endif
