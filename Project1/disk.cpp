#include <iostream>
#include <fstream>
#include <string>
#include <climits>   // INT_MIN
#include <stdlib.h>  //abs, atoi
#include <vector>
#include "thread.h"

using namespace std;

struct Node
{
	int track;
	int requester;
	Node *next;
};

struct queue
{
	int count = 0; // number of Nodes in the queue
	int current_track = 0; // latest return Node, initialize with 0
	int max_que; // maximum Nodes can be in the queue
	Node *head;
	void enqueue(int track, int requester);
	Node dequeue();
	void print();
};

int alive_thread = 0; // indicates numbers of threads that are waiting for served
int numberinput; // number of inputfile
queue q;
mutex mutex1;
cv deque_cv, enque_cv;
vector<bool> deadthread; // check the requester have finished all their job
vector<bool> can_serve; // check whether a track of a requester is in the queue

// purpose of print out data in queue to debug
void queue::print()
{
	Node *ptr = this->head->next;
	cout<<"count: "<<this->count<<"; ";
	while(ptr != NULL)
	{
		cout << ptr->requester<< ", " << ptr->track<<"; ";
		ptr = ptr->next;
	}
	cout<<endl;
}

void queue::enqueue(int track, int requester)
{
	//add new element to the queue
	Node* newelement = new Node;
	newelement->track = track;
	newelement->requester = requester;
	if(this->head->next == NULL)
	{
		newelement->next = NULL;
		head->next = newelement;
	}
	else
	{
		Node *small = this->head;
		Node *large = this->head->next;
		while(large != NULL && track > large->track)
		{
			large = large->next;
			small = small->next;
		}
		if(large == NULL)
		{
			newelement->next = NULL;
			small->next = newelement;
		}
		else //(track < large->track)
		{
			small->next = newelement;
			newelement->next = large;
		}
	}
	cout << "requester " << requester << " track " << track << endl;
	this->count++;
}

Node queue::dequeue()
{
	//remove item from queue
	Node *large = this->head->next;
	Node *small = this->head;
	Node answer;
	while(large->next != NULL && this->current_track > large->next->track)
	{
		large = large->next;
		small = small->next;
	}
	if(large->next == NULL)
	{
		answer = (*large);
		small->next = NULL;
		delete large;
	}
	else
	{
		int a = abs(large->next->track - this->current_track);
		int b = abs(large->track - this->current_track);
		if(a < b) // close to large->next
		{
			Node *tmptr = large->next;
			answer = *(tmptr);
			large->next = large->next->next;
			delete tmptr;
		}
		else
		{
			answer = *(large);
			small->next = large->next;
			delete large;
		}
	}
	this->current_track = answer.track;
	//return removed item
	this->count--;
	return answer;
}

void request(char *a)
{
	// open file
	string filename = (char *)a;
	ifstream f;
	f.open(filename);
	mutex1.lock();
	// read track from input file
	if(f.is_open())
	{
		string line;
		int length = filename.find_last_of(".") - filename.find_first_of(".") - 3;
		int requester = atoi(filename.substr(7, length).c_str());
		
		while(getline(f, line))
		{
			int track = atoi(line.c_str());
			// requester has track in the queue which hasn't been served
			while(!can_serve[requester])
			{
				enque_cv.signal();
				enque_cv.wait(mutex1); //wait
			}
			// queue is full
			while(q.count >= q.max_que)
			{
				deque_cv.signal();
				enque_cv.wait(mutex1); //wait
			}
			q.enqueue(track, requester);
			can_serve[requester] = false;
			deque_cv.signal();
			enque_cv.signal();
		}
		f.close();
		deadthread[requester] = true;
	}
	mutex1.unlock();
}

void service(void *a)
{
	mutex1.lock();
	// when it still has requester to be served
	while(alive_thread > 0)
	{
		// queue is not full and alive thread is more than queue capacity
		while(q.count < q.max_que && alive_thread >= q.max_que)
		{
			deque_cv.wait(mutex1);
		}
		// queue is not empty
		if (q.count > 0)
		{
			Node n = q.dequeue();
			cout<<"service requester "<< n.requester <<" track "<< n.track << endl;
			can_serve[n.requester] = true;
			// this requester doesn't have any other track need to be served
			if (deadthread[n.requester])
			{
				alive_thread--;
				// capacity = max(capacity, alive thread)
				q.max_que = (q.max_que > alive_thread)? alive_thread: q.max_que;
			}
			enque_cv.signal();
		}
	}
	mutex1.unlock();
}

// handling initialization and start the threads
void start(void **argv)
{
	deadthread = vector<bool>(numberinput-2);
	can_serve = vector<bool>(numberinput-2);
	for(int i=0; i<numberinput-2; i++)
	{
		deadthread[i] = false;
		can_serve[i] = true;
	}
	// initialize queue;
	q.max_que = atoi((char*)argv[1]);
	Node *head = new Node;
	head->next = NULL;
	head->track = INT_MIN;
	q.head = head;
	// threads requesters
	for(int i=2; i<numberinput; i++)
	{
		char* filename = (char*) argv[i];
		thread req ((thread_startfunc_t) request, (char *) filename);
		alive_thread++;
	}
	// thread service
	thread serve ((thread_startfunc_t) service,(void *)0);
}

int main(int argc, char **argv)
{
	numberinput = argc;
	if (argc >= 3)
	{
		cpu::boot((thread_startfunc_t) start, (char **) argv, 0);
	}
	else
	{
		cout<<"Please input with file names" <<endl;
	}
}
