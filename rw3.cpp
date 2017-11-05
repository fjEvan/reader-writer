// modified  20171104  Jiao Feng  
/* 
   pass the number of writer threads to be started 
   to the two updating functions in readerwriter.h 
   from command line user input or default value 

   places modified (not many, only writer funtions): 
   all the relevant funtions that need the new parameter
   (commented with NEW!...) 
*/

#include <stdlib.h>
#include <time.h>

#include <iostream>
#include <algorithm>
#include <string>
#include <list>
#include <algorithm>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <utility>

#include "semaphore.h"
#include "readerwriter.h"

using namespace std;

/*
 ******************************************************************************
 * This is a readers/writers test application which maintains a shared linked
 * list of string/integer pairs.  It starts a specified number of reading and
 * writing threads which each perform a sepcified number of operations on
 * the list.  In addition, a monitor thread runs which periodically reports
 * the number of completed operations.  Run with command-line options to
 * control the program's behavior:
 *  -q    Run more quietly.  Want this for most measurements.
 *  -r N  Specify the number of reading threads, default 10.
 *  -w N  Specify the number of writing threads, default 10.
 *  -n N  Specify the number operations (reads or writes) performed by each
 *        thread.
 ******************************************************************************
 */

/*
 * This is a hack to lock cout printing by lines.  It's ugly and not too
 * general, but it seems to work for this.  Use coulock() << ... whatever ...
 * << unlendl; to print a line to cout while holding a mutex so one line won't
 * be interrupted by another.
 */
static recursive_mutex io_mux;
inline ostream &coulock() {
	io_mux.lock();
	return cout;
}
class unlocking_endl_class { };
unlocking_endl_class unlendl;
inline ostream & operator<<(ostream &s, unlocking_endl_class &e)
{
	s << endl;
	io_mux.unlock();
	return s;
}

/*
 * Call rand() under a mutex lock.  Not safe to call from threads unguarded.
 */
int locked_rand()
{
	static mutex mux;
	unique_lock<mutex> locker(mux);
	return rand();
}

/*
 * Choose one of the strings from a list at random.
 */
string rand_name()
{
	string names[] = {
		"international", "consolidated", "confederated", "amalgomated",
		"fubar", "reinitialized", "discombobulated", "missing", 
		"fiduciary", "exasperated", "laminated", "embalmed" };
	int which = locked_rand() % (sizeof names / sizeof names[0]);
	return names[which];
}

/*
 * This is the shared data structure, a un-ordered linked list of names and
 * values.
 */
class data_node {
private:
	int m_value;
	string m_name;
public:
	data_node(string n, int v = 0): m_name(n), m_value(v) { }
	int value() { return m_value; }
	void incval(int i) { m_value += i; }
	string name() { return m_name; }
};
list<data_node> data_list;

/*
 * The reader-writer control object.  This contains the methods for
 * readers and writers to enter and leave the reading and writing sections.
 */
readerwriter rwctl;

/*
 * Find a random name.  If not quiet, prints what it does.
 */
void finder(bool quiet)
{
	// Choose a name at random to look for.
	string tofind = rand_name();

	rwctl.enter_read();

	// Scan for the name.
	auto scan = data_list.begin();
	while(scan != data_list.end()) {
		if(tofind == scan->name()) break;
		++scan;
	}

	// Print results.
	if(!quiet) {
		if(scan == data_list.end())
			coulock() << tofind << " is not listed"
				  << unlendl;
		else
			coulock() << scan->name() << " is valued at "
				  << scan->value() << unlendl;
	}
	
	rwctl.leave_read();
}

/*
 * Find the average value in the list.  If not quiet, print it.
 */
void av(bool quiet)
{
	int sum = 0;

	rwctl.enter_read();

	// Go through the list and make the sum.
	auto scan = data_list.begin();
	for(; scan != data_list.end(); ++scan)
		sum += scan->value();

	// Copy the size to local data so we can release.
	int ct = data_list.size();

	rwctl.leave_read();

	// Print results.
	if(!quiet) {
		if(ct > 0)
			coulock() << "Average is "
				  << (double)sum / (double)ct
				  << unlendl;
		else
			coulock() << "No items to average" << unlendl;
	}
}

/*
 * Find the range.  If not quiet, print the result.
 */
void range(bool quiet)
{
	rwctl.enter_read();

	// Is there anything?
	if(data_list.size() == 0) {
		// No.  Bail.
		rwctl.leave_read();
		if(!quiet)
			coulock() << "No items for range computation."
				  << unlendl;
	} else {
		// Yes. Find the range.
		auto scan = data_list.begin();
		int min = scan->value();
		int max = scan->value();
		for(++scan; scan != data_list.end(); ++scan) {
			if(min > scan->value()) min = scan->value();
			if(max < scan->value()) max = scan->value();
		}
		rwctl.leave_read();

		// Print the result.
		if(!quiet)
			coulock() << "Range is " << min << " to "
				  << max << unlendl;
	}
}

/* This contains a count of reads.  It is only updated occassionally in order
   to reduce overhead.   It is an atomic counter so I can access it from
   different threads without using a mutex. */
atomic<int> read_count(0);

/*
 * Reader thread.  Performs some number of read operations.  Chooses
 * randomly which of finder, av or range to run.
 */
void reader(bool quiet, int cnt)
{
	for(int i = 1; i <= cnt; ++i) {
		// Pick one randomly and run it.
		int choice = locked_rand() % 100;

		if(choice < 20)
			av(quiet);
		else if(choice < 40)
			range(quiet);
		else
			finder(quiet);

		// Sometimes we update the count.
		if(i % 256 == 0) read_count += 256;
	}
	read_count += cnt % 256;
}

/*
 *************** Some updaters. *******************
 */

/*
 * Update a random item.  Find the (an) item given by which, and add
 * the increment.
 */
void update(bool quiet, int increment, string which, int nwriter)
{                                   // NEW! add a parameter nwriter
	bool created = false;

	rwctl.enter_write(nwriter);

	// Find in the list. 
	auto scan = data_list.begin();
	for(; scan != data_list.end(); ++scan) {
		if(which == scan->name()) {
			// If found, increment.
			scan->incval(increment);
			break;
		}
	}

	// If not found add to the front.
	if(scan == data_list.end()) {
		// Add to the front of the list.  
		data_list.push_front(data_node(which, increment));
		created = true;
	}

	rwctl.leave_write();

	// Fess up.
	if(!quiet) 
		coulock() << which << " "
			  <<  (created ? "created with" : "given") << " "
			  << increment << unlendl;
}

/*
 * Delete a random item, if present.  If it picks a name which is not in the
 * list, does nothing.
 */
void remove_node(bool quiet, int nwriter)
{                           // NEW! add a parameter nwriter
	// Choose a random name.
	string which = rand_name();

	rwctl.enter_write(nwriter);

	// Scan the list to look for the randomly-chosen name. 
	auto scan = data_list.begin();
	while(scan != data_list.end() && scan->name() != which)
		++scan;

	// If something was found, remove it. 
	bool was_deleted = false;
	if(scan != data_list.end()) {
		data_list.erase(scan);
		was_deleted = true;
	}

	rwctl.leave_write();

	// Let us know. 
	if(!quiet) {
		if(was_deleted)
			coulock() << which << " deleted" << unlendl;
		else
			coulock() << which << " could not be found" << unlendl;
	}
}


/* This contains a count of reads.  It is not always up-to-date */
atomic<int> write_count(0);

/*
 * Update thread.  Performs some number of writing operations.  Chooses
 * randomly which of update or remove_node to call.
 */
void writer(bool quiet, int cnt, int nwriter) 
{                               // NEW! add a parameter nwriter
	for(int i = 1; i <= cnt; ++i) {
		int choice = locked_rand() % 100;

		if(choice < 65)
			update(quiet, locked_rand() % 100 - 10, rand_name(), nwriter);
		else
			remove_node(quiet, nwriter);

		if(i % 256 == 0) {
			write_count += 256;
		}
	}
	write_count += cnt % 256;
}

/*
 * This monitors progress. 
 */
atomic<bool> monitor_stop(false);
void monitor(int nread, int nwrite)
{
	while(!monitor_stop) {
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		int rc = read_count;
		int wc = write_count;
		coulock() << "=== " << rc << " reads (" << 100.0*rc/nread
			  << "%), " << wc << " writes (" << 100.0*wc/nwrite
			  << "%) ===" << unlendl;
	}
}

void usage()
{
	cerr << "Flags: -q[uiet], -r nreader, -w nwriter "
	     << "-n numop" << endl;
	exit(1);
}

/*
 * The main starts the threads that start the test threads, then wait it all.
 */
int main(int argc, char **argv)
{
	// Parameters
	bool quiet = false;	// Threads should shut up.
	int nreader = 10;	// Number of reading threads.
	int nwriter = 10;	// Size of update threads.
	int nop = 10;		// Number of operations for each thread.
	//int nwriter = 15;

	// Collect the parameter values.
	for(argc--, argv++; argc > 0; argc--, argv++) {
		if(string(*argv) == "-q") {
			quiet = true;
		}
		else if((*argv)[0] == '-' && argc > 1) {
			--argc;
			switch((*argv++)[1]) {
			case 'r':
				nreader = atoi(*argv);
				break;
			case 'w':
				nwriter = atoi(*argv);
				break;
			case 'n':
				nop = atoi(*argv);
				break;
			default:
				usage();
			}
		} else {
			usage();
		}
	}

	cout << "Running " << nreader << " read threads and "
	     << nwriter << " update threads, " << nop 
	     << " operations each" << endl;

	srand(time(NULL));

	// Start the reading and writing threads.  Start them alternately,
	// and collect in a list.
	int totthreads = nreader + nwriter;
	list<thread> threads;
	for(int i = 0; i < max(nreader,nwriter); ++i) {
		if(i < nreader)
			threads.push_back(thread(reader, quiet, nop));
		if(i < nwriter)
			threads.push_back(thread(writer, quiet, nop, nwriter));
	}                          // NEW! pass the new parameter nwriter

	// Start the progress monitor thread.
	thread monthread(monitor, nop*nreader, nop*nwriter);

	// Run join on all the reader/writer threads.
	for(auto & t: threads) {
		t.join();
	}

	// Now that they are done, ask the monitor to stop, then join it.
	monitor_stop = true;
	monthread.join();
}
